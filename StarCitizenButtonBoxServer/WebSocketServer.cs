using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Text.Json;
using Fleck;

namespace StarCitizenButtonBoxServer;


public sealed class ButtonBoxServer : IDisposable
{
    // Set to true to log every WebSocket frame -- handy when something stops working and you can't tell why.
    public static bool VerboseWireLogging { get; set; } = false;

    const int WireMaxChars = 600;

    readonly BindingManager _bindings;
    readonly InputHandler? _keyboard;
    readonly MediaHandler _media;
    readonly ConcurrentDictionary<Guid, IWebSocketConnection> _clients = new();

    WebSocketServer? _server;
    Timer? _heartbeatTimer;
    int _port;

    public int Port => _port;

    public event EventHandler<string>? StatusLog;
    public event EventHandler? ClientsChanged;

    public ButtonBoxServer(BindingManager bindings, InputHandler? keyboard,
        MediaHandler media)
    {
        _bindings = bindings;
        _keyboard = keyboard;
        _media = media;
    }

    public void Start(int port)
    {
        Stop();
        _port = port;
        FleckLog.Level = LogLevel.Info;
        _server = new WebSocketServer($"ws://0.0.0.0:{port}");
        _server.ListenerSocket.NoDelay = true;
        _server.Start(socket =>
        {
            socket.OnOpen = () => OnOpen(socket);
            socket.OnClose = () => OnClose(socket);
            socket.OnMessage = m => OnMessage(socket, m);
            socket.OnError = e => Log($"WebSocket error: {e.Message}");
        });

        Log($"WebSocket listening on {GetLocalIPv4()}:{port}");

        _heartbeatTimer = new Timer(_ => PingAllClients(), null,
            TimeSpan.FromSeconds(15), TimeSpan.FromSeconds(15));
    }

    public void Stop()
    {
        _heartbeatTimer?.Dispose();
        _heartbeatTimer = null;
        foreach (var kv in _clients)
            kv.Value.Close(1001);
        _clients.Clear();
        _server?.Dispose();
        _server = null;
    }

    void OnOpen(IWebSocketConnection socket)
    {
        _clients[socket.ConnectionInfo.Id] = socket;
        Log($"Client connected ({_clients.Count} total).");
        ClientsChanged?.Invoke(this, EventArgs.Empty);
        _ = SendMediaSnapshotAsync(socket);
    }

    void OnClose(IWebSocketConnection socket)
    {
        _clients.TryRemove(socket.ConnectionInfo.Id, out _);
        Log($"Client disconnected ({_clients.Count} total).");
        ClientsChanged?.Invoke(this, EventArgs.Empty);
    }

    void OnMessage(IWebSocketConnection socket, string message)
    {
        var peer = socket.ConnectionInfo.Id.ToString("N")[..8];
        if (VerboseWireLogging)
            Log($"RECV [{peer}] {TruncateForLog(message)}");

        try
        {
            using var doc = JsonDocument.Parse(message);
            var root = doc.RootElement;
            if (!root.TryGetProperty("type", out var typeProp))
            {
                Log($"RECV [{peer}] missing JSON property 'type'");
                return;
            }

            var type = typeProp.GetString() ?? "";

            switch (type)
            {
                case "command":
                    if (root.TryGetProperty("commandId", out var cid))
                        _ = HandleCommandAsync(cid.GetString() ?? "");
                    break;
                case "media":
                    if (root.TryGetProperty("action", out var act))
                        _ = HandleMediaAsync(act.GetString() ?? "");
                    break;
                case "context_change":
                    if (root.TryGetProperty("mode", out var modeProp))
                    {
                        var mode = modeProp.GetString() ?? "";
                        Log($"Context change from [{peer}], mode={mode}");
                        BroadcastContext(mode);
                    }
                    else
                        Log($"RECV [{peer}] context_change missing 'mode'");
                    break;
                default:
                    Log($"Unknown message type: {type}");
                    break;
            }
        }
        catch (Exception ex)
        {
            Log($"Bad message: {ex.Message}");
        }
    }

    async Task HandleCommandAsync(string commandId)
    {
        var entry = _bindings.GetBinding(commandId);
        if (entry == null)
        {
            Log($"CMD '{commandId}' -> no binding found.");
            return;
        }

        var mods = new System.Text.StringBuilder();
        if (entry.LeftCtrl)   mods.Append("LCtrl+");
        if (entry.RightCtrl)  mods.Append("RCtrl+");
        if (entry.LeftShift)  mods.Append("LShift+");
        if (entry.RightShift) mods.Append("RShift+");
        if (entry.LeftAlt)    mods.Append("LAlt+");
        if (entry.RightAlt)   mods.Append("RAlt+");
        string keyPart = entry.MouseInputAction != MouseInputAction.None ? entry.MouseInputAction.ToString()
                       : !string.IsNullOrWhiteSpace(entry.Key) ? entry.Key
                       : "(nothing)";
        Log($"CMD '{commandId}' -> {mods}{keyPart} hold={entry.HoldTimeMs}ms");

        try
        {
            if (_keyboard == null)
            {
                Log($"CMD '{commandId}' -> Interception driver not loaded, skipping.");
                return;
            }
            await _keyboard.ExecuteAsync(entry);
        }
        catch (Exception ex)
        {
            Log($"Execute failed for '{commandId}': {ex.Message}");
        }
    }

    async Task HandleMediaAsync(string action)
    {
        Log($"Media action: {action}");
        var ok = await _media.HandleAction(action);
        if (!ok)
            Log("No active media session or action failed.");
    }

    public void BroadcastContext(string mode)
    {
        var payload = JsonSerializer.Serialize(new { type = "context_change", mode }, WireJsonOptions);
        var snapshot = _clients.Values.ToArray();
        if (VerboseWireLogging)
            Log($"SEND broadcast context_change ({snapshot.Length} peer(s)): {TruncateForLog(payload)}");

        foreach (var c in snapshot)
            SendToPeer(c, payload, "broadcast context_change");

        Log($"Context broadcast complete: mode={mode}, peers={snapshot.Length}");
    }

    public void BroadcastMedia(MediaSnapshot snap)
    {
        var payload = JsonSerializer.Serialize(new
        {
            type = "media_update",
            title = snap.Title,
            artist = snap.Artist,
            album = snap.Album,
            thumbnailBase64 = snap.ThumbnailBase64Png ?? ""
        }, WireJsonOptions);

        var snapshot = _clients.Values.ToArray();
        Log($"Media update: '{snap.Title}' / '{snap.Artist}' -> {snapshot.Length} client(s)");
        if (VerboseWireLogging)
            Log($"SEND broadcast media_update ({snapshot.Length} peer(s)): {TruncateForLog(payload)}");

        foreach (var c in snapshot)
            SendToPeer(c, payload, "broadcast media_update");
    }

    async Task SendMediaSnapshotAsync(IWebSocketConnection socket)
    {
        try
        {
            var snap = await _media.GetSnapshotAsync();
            Log($"Initial media snapshot: '{snap.Title}' / '{snap.Artist}'");
            var payload = JsonSerializer.Serialize(new
            {
                type = "media_update",
                title = snap.Title,
                artist = snap.Artist,
                album = snap.Album,
                thumbnailBase64 = snap.ThumbnailBase64Png ?? ""
            }, WireJsonOptions);
            SendToPeer(socket, payload, "initial media snapshot");
        }
        catch (Exception ex)
        {
            Log($"Initial media snapshot failed: {ex.Message}");
        }
    }

    static readonly JsonSerializerOptions WireJsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase
    };

    string TruncateForLog(string raw)
    {
        if (string.IsNullOrEmpty(raw)) return raw;
        if (raw.Length <= WireMaxChars)
            return raw;
        return raw[..WireMaxChars] + $" ... ({raw.Length} chars total)";
    }

    void SendToPeer(IWebSocketConnection socket, string payload, string reason)
    {
        var peer = socket.ConnectionInfo.Id.ToString("N")[..8];
        try
        {
            _ = socket.Send(payload);
            if (VerboseWireLogging)
                Log($"SEND [{peer}] {reason}: {TruncateForLog(payload)}");
        }
        catch (Exception ex)
        {
            Log($"SEND FAILED [{peer}] {reason}: {ex.Message}");
        }
    }

    void PingAllClients()
    {
        foreach (var kv in _clients.ToArray())
        {
            try { kv.Value.SendPing([]); }
            catch { /* OnError → OnClose will fire and remove the client */ }
        }
    }

    void Log(string line) => StatusLog?.Invoke(this, $"[{DateTime.Now:HH:mm:ss}] {line}");

    public IReadOnlyCollection<string> GetClientSummaries() =>
        _clients.Values.Select(c => c.ConnectionInfo.ClientIpAddress ?? "?").ToList();

    public static string GetLocalIPv4()
    {
        foreach (var ni in Dns.GetHostAddresses(Dns.GetHostName()))
        {
            if (ni.AddressFamily == AddressFamily.InterNetwork && !IPAddress.IsLoopback(ni))
                return ni.ToString();
        }

        return "127.0.0.1";
    }

    public void Dispose() => Stop();
}
