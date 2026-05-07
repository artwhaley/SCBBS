/*
  ESPCitizen - ESP32-S3 Websocket client to send commands to SCBBS Server
  ------------------------------------------------
  - Joins Wi-Fi
  - finds the first _starcommander._tcp service via mDNS
  - connects over websocket
  - sends JSON command messages on button press
 
*/

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebSocketsClient.h>
#include <stdio.h>

// --------------------------
// User config
// --------------------------

const char WIFI_SSID[] = "YOUR_WIFI_SSID";
const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// Optional fallback if mDNS is unavailable.
// Leave as empty string to require mDNS.
const char FALLBACK_SERVER_HOST[] = "";
const uint16_t FALLBACK_SERVER_PORT = 8795;

// GPIO pins (INPUT_PULLUP, button press = LOW). Update for your board/wiring.
//Mining interface example
const uint8_t PIN_FIRE_LASER = 15;
const uint8_t PIN_FRACTURE = 2;
const uint8_t PIN_MOD_1 = 4;
const uint8_t PIN_MOD_2 = 16;
const uint8_t PIN_MOD_3 = 17;
const uint8_t PIN_NAVMODE = 5;
const uint8_t PIN_SCMMODE = 18;
const uint8_t PIN_SCAN_MODE = 19;
const uint8_t PIN_SCAN_ANGLE_PLUS = 21;
const uint8_t PIN_SCAN_ANGLE_MINUS = 22;
const uint8_t PIN_EXECUTE_SCAN = 23;
const uint8_t PIN_TOGGLE_PING = 13;
const uint8_t PIN_PING_ONCE = 12;

const uint8_t BUTTON_COUNT = 13;

// --------------------------
// Network / client settings
// --------------------------
const unsigned long WIFI_RETRY_MS = 5000;
const unsigned long MDNS_RETRY_MS = 7000;
const unsigned long WS_RETRY_START_MS = 1000;
const unsigned long WS_RETRY_MAX_MS = 30000;
const unsigned long DEBOUNCE_MS = 45;

const uint8_t CMD_QUEUE_SIZE = 20;
const uint8_t CMD_JSON_MAX = 96;

struct ButtonMap {
  uint8_t pin;
  const char* commandId;
  bool lastRaw;
  bool stable;
  unsigned long lastChangeMs;
};

struct CommandQueue {
  char payload[CMD_QUEUE_SIZE][CMD_JSON_MAX];
  uint8_t head;
  uint8_t tail;
  uint8_t count;
};

// --------------------------
// Globals
// --------------------------
WebSocketsClient wsClient;
CommandQueue sendQueue = {0};

ButtonMap buttons[BUTTON_COUNT] = {
  {PIN_FIRE_LASER, "mining_fire_laser", true, true, 0},
  {PIN_FRACTURE, "salvage_toggle_fracture", true, true, 0},
  {PIN_MOD_1, "mining_activate_module_1", true, true, 0},
  {PIN_MOD_2, "mining_activate_module_2", true, true, 0},
  {PIN_MOD_3, "mining_activate_module_3", true, true, 0},
  {PIN_NAVMODE, "modes_master_mode_nav", true, true, 0},
  {PIN_SCMMODE, "modes_master_mode_scm", true, true, 0},
  {PIN_SCAN_MODE, "sensors_scanning_mode", true, true, 0},
  {PIN_SCAN_ANGLE_PLUS, "sensors_increase_scanning_angle", true, true, 0},
  {PIN_SCAN_ANGLE_MINUS, "sensors_decrease_scanning_angle", true, true, 0},
  {PIN_EXECUTE_SCAN, "sensors_scannow", true, true, 0},
  {PIN_TOGGLE_PING, "sensors_ping", true, true, 0},
  {PIN_PING_ONCE, "sensors_ping", true, true, 0},
};

bool mdnsReady = false;
bool hasServer = false;
IPAddress serverIp;
uint16_t serverPort = FALLBACK_SERVER_PORT;
String serverHost;

unsigned long lastWifiAttemptMs = 0;
unsigned long lastMdnsAttemptMs = 0;
unsigned long nextWsAttemptMs = 0;
unsigned long wsRetryMs = WS_RETRY_START_MS;
bool wifiWasConnected = false;
bool wsConnected = false;
unsigned long lastStatusMs = 0;


void connectWifiIfNeeded();
void ensureMdnsAndDiscover();
void connectWebSocketIfNeeded();
void checkButtons();
void flushQueue();
void enqueueCommand(const char* commandId);
bool queuePush(const char* payload);
bool queuePop(char* out);
void onWsEvent(WStype_t type, uint8_t* payload, size_t length);
bool parseIpString(const char* host, IPAddress& outIp);
void printStatusLine(const char* line);

// --------------------------
// Setup
// --------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_FIRE_LASER, INPUT_PULLUP);
  pinMode(PIN_FRACTURE, INPUT_PULLUP);
  pinMode(PIN_MOD_1, INPUT_PULLUP);
  pinMode(PIN_MOD_2, INPUT_PULLUP);
  pinMode(PIN_MOD_3, INPUT_PULLUP);
  pinMode(PIN_NAVMODE, INPUT_PULLUP);
  pinMode(PIN_SCMMODE, INPUT_PULLUP);
  pinMode(PIN_SCAN_MODE, INPUT_PULLUP);
  pinMode(PIN_SCAN_ANGLE_PLUS, INPUT_PULLUP);
  pinMode(PIN_SCAN_ANGLE_MINUS, INPUT_PULLUP);
  pinMode(PIN_EXECUTE_SCAN, INPUT_PULLUP);
  pinMode(PIN_TOGGLE_PING, INPUT_PULLUP);
  pinMode(PIN_PING_ONCE, INPUT_PULLUP);

  wsClient.onEvent(onWsEvent);
  wsClient.setReconnectInterval(0);

  WiFi.mode(WIFI_STA);
  connectWifiIfNeeded();
}


void loop() {
  connectWifiIfNeeded();
  if (WiFi.status() != WL_CONNECTED) {
    wsClient.loop();
    delay(25);
    return;
  }

  if (!wifiWasConnected) {
    wifiWasConnected = true;
    printStatusLine("WiFi connected");
    if (!mdnsReady) {
      mdnsReady = MDNS.begin("espcitizen");
      if (!mdnsReady) {
        printStatusLine("mDNS start failed");
      }
    }
    lastMdnsAttemptMs = 0;
  }

  if (WiFi.status() == WL_CONNECTED) {
    ensureMdnsAndDiscover();
  }

  connectWebSocketIfNeeded();
  wsClient.loop();

  checkButtons();
  flushQueue();

  if (millis() - lastStatusMs > 1000) {
    lastStatusMs = millis();
    if (wsConnected) {
      Serial.print("WS connected to ");
      Serial.print(serverHost);
      Serial.print(":");
      Serial.println(serverPort);
    } else if (hasServer) {
      Serial.print("Server discovered at ");
      Serial.print(serverIp);
      Serial.print(":");
      Serial.println(serverPort);
    } else {
      Serial.println("Waiting for server discovery...");
    }
  }

  delay(10);
}


void connectWifiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;

  wifiWasConnected = false;
  wsConnected = false;
  mdnsReady = false;
  hasServer = false;

  if (millis() - lastWifiAttemptMs < WIFI_RETRY_MS) return;
  lastWifiAttemptMs = millis();

  printStatusLine("Connecting WiFi...");
  WiFi.disconnect(true, true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void ensureMdnsAndDiscover() {
  if (hasServer) return;
  if (!mdnsReady) {
    mdnsReady = MDNS.begin("espcitizen");
    if (!mdnsReady) {
      printStatusLine("mDNS start failed");
      return;
    }
  }
  if (millis() - lastMdnsAttemptMs < MDNS_RETRY_MS) return;
  lastMdnsAttemptMs = millis();

  printStatusLine("Scanning for _starcommander._tcp service...");
  if (MDNS.queryService("starcommander", "tcp")) {
    serverIp = MDNS.IP(0);
    serverPort = MDNS.port(0);
    serverHost = MDNS.hostname(0);
    if (serverIp != IPAddress(0, 0, 0, 0) && serverPort > 0) {
      hasServer = true;
      printStatusLine("mDNS found first server");
      wsRetryMs = WS_RETRY_START_MS;
        nextWsAttemptMs = 0;
      return;
    }
  }

  if (strlen(FALLBACK_SERVER_HOST) > 0 && parseIpString(FALLBACK_SERVER_HOST, serverIp)) {
    hasServer = true;
    serverPort = FALLBACK_SERVER_PORT;
    serverHost = FALLBACK_SERVER_HOST;
    printStatusLine("mDNS found nothing; using manual fallback");
  } else {
    hasServer = false;
    printStatusLine("No server found yet");
  }
}

void connectWebSocketIfNeeded() {
  if (wsConnected || !hasServer) return;
  if (millis() < nextWsAttemptMs) return;

  String host = serverIp.toString();
  wsClient.begin(host.c_str(), serverPort, "/");
  wsClient.setReconnectInterval(0);
  nextWsAttemptMs = millis() + wsRetryMs;
  wsRetryMs = min((unsigned long)(wsRetryMs * 2), WS_RETRY_MAX_MS);
}


void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      wsRetryMs = WS_RETRY_START_MS;
      printStatusLine("WebSocket disconnected");
      break;

    case WStype_CONNECTED:
      wsConnected = true;
      wsRetryMs = WS_RETRY_START_MS;
      nextWsAttemptMs = 0;
      printStatusLine("WebSocket connected");
      break;

    case WStype_TEXT:
      (void)payload;
      (void)length;
      break;

    default:
      break;
  }
}

void checkButtons() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    bool reading = digitalRead(buttons[i].pin);
    if (reading != buttons[i].lastRaw) {
      buttons[i].lastRaw = reading;
      buttons[i].lastChangeMs = now;
    }

    if ((now - buttons[i].lastChangeMs) >= DEBOUNCE_MS && reading != buttons[i].stable) {
      buttons[i].stable = reading;
      if (!reading) {
        enqueueCommand(buttons[i].commandId);
      }
    }
  }
}

void enqueueCommand(const char* commandId) {
  char payload[CMD_JSON_MAX];
  snprintf(payload, sizeof(payload), "{\"type\":\"command\",\"commandId\":\"%s\"}", commandId);
  if (queuePush(payload)) {
    Serial.print("Queued ");
    Serial.println(commandId);
  } else {
    Serial.println("Queue full; dropped oldest command");
  }
}

bool queuePush(const char* payload) {
  if (sendQueue.count >= CMD_QUEUE_SIZE) {
    // Drop oldest and keep latest command.
    sendQueue.head = (sendQueue.head + 1) % CMD_QUEUE_SIZE;
    sendQueue.count--;
  }

  size_t len = strnlen(payload, CMD_JSON_MAX);
  if (len >= CMD_JSON_MAX) {
    len = CMD_JSON_MAX - 1;
  }
  strncpy(sendQueue.payload[sendQueue.tail], payload, len);
  sendQueue.payload[sendQueue.tail][len] = '\0';

  sendQueue.tail = (sendQueue.tail + 1) % CMD_QUEUE_SIZE;
  sendQueue.count++;
  return true;
}

bool queuePop(char* out) {
  if (sendQueue.count == 0) return false;

  strcpy(out, sendQueue.payload[sendQueue.head]);
  sendQueue.head = (sendQueue.head + 1) % CMD_QUEUE_SIZE;
  sendQueue.count--;
  return true;
}

void flushQueue() {
  if (!wsConnected) return;

  char msg[CMD_JSON_MAX];
  if (queuePop(msg)) {
    wsClient.sendTXT(msg);
  }
}

bool parseIpString(const char* host, IPAddress& outIp) {
  int a, b, c, d;
  if (sscanf(host, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
    outIp = IPAddress(a, b, c, d);
    return true;
  }
  return false;
}

void printStatusLine(const char* line) {
  Serial.print("[ESPCitizen] ");
  Serial.println(line);
}
