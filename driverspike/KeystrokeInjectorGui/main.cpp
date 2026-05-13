#include <windows.h>
#include <cfgmgr32.h>

#include <cwctype>
#include <cstdarg>
#include <string>

#include "driverspike-common.h"
#include "../TheSpikeyDriver/Public.h"

#define IDC_KEY_EDIT 1001
#define IDC_START_BUTTON 1002
#define IDC_CANCEL_BUTTON 1003
#define IDC_STATUS_TEXT 1004
#define IDC_HELP_TEXT 1005
#define IDC_STATS_BUTTON 1006
#define IDC_DIAGNOSTICS_EDIT 1007
#define COUNTDOWN_TIMER_ID 2001

static HINSTANCE g_Instance;
static HWND g_KeyEdit;
static HWND g_StartButton;
static HWND g_CancelButton;
static HWND g_StatusText;
static HWND g_StatsButton;
static HWND g_DiagnosticsEdit;
static int g_SecondsRemaining;
static UCHAR g_PendingUsage;
static UCHAR g_PendingModifier;

struct ControlOpenResult {
    HANDLE Handle = INVALID_HANDLE_VALUE;
    ULONG InterfaceListLength = 0;
    ULONG InterfaceCount = 0;
    DWORD LastCreateFileError = ERROR_SUCCESS;
    std::wstring FirstPath;
    std::wstring Message;
};

static std::wstring Trim(const std::wstring& text)
{
    size_t first = 0;
    while (first < text.size() && iswspace(text[first])) {
        first++;
    }

    size_t last = text.size();
    while (last > first && iswspace(text[last - 1])) {
        last--;
    }

    return text.substr(first, last - first);
}

static std::wstring ToLower(std::wstring text)
{
    for (wchar_t& ch : text) {
        ch = (wchar_t)towlower(ch);
    }
    return text;
}

static void SetStatus(const wchar_t* text)
{
    SetWindowTextW(g_StatusText, text);
}

static void SetDiagnostics(const std::wstring& text)
{
    SetWindowTextW(g_DiagnosticsEdit, text.c_str());
}

static void AppendFormat(std::wstring* text, const wchar_t* format, ...)
{
    wchar_t buffer[512];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, format, args);
    va_end(args);

    text->append(buffer);
}

static const wchar_t* DescribeCommandState(unsigned int state)
{
    switch (state) {
    case THESPIKEYDRIVER_COMMAND_STATE_IDLE:
        return L"idle";
    case THESPIKEYDRIVER_COMMAND_STATE_PENDING_KEY_DOWN:
        return L"queued: next report is key-down";
    case THESPIKEYDRIVER_COMMAND_STATE_PENDING_KEY_UP:
        return L"active: next report is key-up";
    default:
        return L"unknown";
    }
}

static bool ParseUnsignedByte(const std::wstring& text, UCHAR* value)
{
    wchar_t* end = nullptr;
    unsigned long parsed = wcstoul(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != L'\0' || parsed > 0xFF) {
        return false;
    }

    *value = (UCHAR)parsed;
    return true;
}

static bool LookupNamedKey(const std::wstring& token, UCHAR* usage)
{
    const std::wstring lower = ToLower(token);

    struct NamedKey {
        const wchar_t* Name;
        UCHAR Usage;
    };

    static const NamedKey keys[] = {
        { L"enter", 0x28 }, { L"return", 0x28 }, { L"esc", 0x29 }, { L"escape", 0x29 },
        { L"backspace", 0x2A }, { L"tab", 0x2B }, { L"space", 0x2C },
        { L"capslock", 0x39 }, { L"caps lock", 0x39 },
        { L"f1", 0x3A }, { L"f2", 0x3B }, { L"f3", 0x3C }, { L"f4", 0x3D },
        { L"f5", 0x3E }, { L"f6", 0x3F }, { L"f7", 0x40 }, { L"f8", 0x41 },
        { L"f9", 0x42 }, { L"f10", 0x43 }, { L"f11", 0x44 }, { L"f12", 0x45 },
        { L"insert", 0x49 }, { L"home", 0x4A }, { L"pageup", 0x4B }, { L"page up", 0x4B },
        { L"delete", 0x4C }, { L"del", 0x4C }, { L"end", 0x4D },
        { L"pagedown", 0x4E }, { L"page down", 0x4E },
        { L"right", 0x4F }, { L"left", 0x50 }, { L"down", 0x51 }, { L"up", 0x52 },
        { L"numlock", 0x53 }, { L"num lock", 0x53 }
    };

    for (const NamedKey& key : keys) {
        if (lower == key.Name) {
            *usage = key.Usage;
            return true;
        }
    }

    return false;
}

static bool MapCharacter(wchar_t ch, UCHAR* usage, UCHAR* modifier)
{
    *modifier = 0;

    if (ch >= L'a' && ch <= L'z') {
        *usage = (UCHAR)(0x04 + (ch - L'a'));
        return true;
    }

    if (ch >= L'A' && ch <= L'Z') {
        *usage = (UCHAR)(0x04 + (ch - L'A'));
        *modifier = 0x02;
        return true;
    }

    if (ch >= L'1' && ch <= L'9') {
        *usage = (UCHAR)(0x1E + (ch - L'1'));
        return true;
    }

    if (ch == L'0') {
        *usage = 0x27;
        return true;
    }

    switch (ch) {
    case L'\r':
    case L'\n': *usage = 0x28; return true;
    case L'\t': *usage = 0x2B; return true;
    case L' ': *usage = 0x2C; return true;
    case L'!': *usage = 0x1E; *modifier = 0x02; return true;
    case L'@': *usage = 0x1F; *modifier = 0x02; return true;
    case L'#': *usage = 0x20; *modifier = 0x02; return true;
    case L'$': *usage = 0x21; *modifier = 0x02; return true;
    case L'%': *usage = 0x22; *modifier = 0x02; return true;
    case L'^': *usage = 0x23; *modifier = 0x02; return true;
    case L'&': *usage = 0x24; *modifier = 0x02; return true;
    case L'*': *usage = 0x25; *modifier = 0x02; return true;
    case L'(': *usage = 0x26; *modifier = 0x02; return true;
    case L')': *usage = 0x27; *modifier = 0x02; return true;
    case L'-': *usage = 0x2D; return true;
    case L'_': *usage = 0x2D; *modifier = 0x02; return true;
    case L'=': *usage = 0x2E; return true;
    case L'+': *usage = 0x2E; *modifier = 0x02; return true;
    case L'[': *usage = 0x2F; return true;
    case L'{': *usage = 0x2F; *modifier = 0x02; return true;
    case L']': *usage = 0x30; return true;
    case L'}': *usage = 0x30; *modifier = 0x02; return true;
    case L'\\': *usage = 0x31; return true;
    case L'|': *usage = 0x31; *modifier = 0x02; return true;
    case L';': *usage = 0x33; return true;
    case L':': *usage = 0x33; *modifier = 0x02; return true;
    case L'\'': *usage = 0x34; return true;
    case L'"': *usage = 0x34; *modifier = 0x02; return true;
    case L'`': *usage = 0x35; return true;
    case L'~': *usage = 0x35; *modifier = 0x02; return true;
    case L',': *usage = 0x36; return true;
    case L'<': *usage = 0x36; *modifier = 0x02; return true;
    case L'.': *usage = 0x37; return true;
    case L'>': *usage = 0x37; *modifier = 0x02; return true;
    case L'/': *usage = 0x38; return true;
    case L'?': *usage = 0x38; *modifier = 0x02; return true;
    default:
        return false;
    }
}

static bool ParseKeystroke(const std::wstring& input, UCHAR* usage, UCHAR* modifier, std::wstring* error)
{
    std::wstring text = Trim(input);
    *usage = 0;
    *modifier = 0;

    if (text.empty()) {
        *error = L"Enter a key first.";
        return false;
    }

    UCHAR parsedUsage = 0;
    if (ParseUnsignedByte(text, &parsedUsage)) {
        *usage = parsedUsage;
        return true;
    }

    size_t plus = text.find(L'+');
    std::wstring keyToken = text;
    while (plus != std::wstring::npos) {
        const std::wstring token = ToLower(Trim(text.substr(0, plus)));
        if (token == L"ctrl" || token == L"control") {
            *modifier |= 0x01;
        } else if (token == L"shift") {
            *modifier |= 0x02;
        } else if (token == L"alt") {
            *modifier |= 0x04;
        } else if (token == L"win" || token == L"gui" || token == L"windows") {
            *modifier |= 0x08;
        } else {
            *error = L"Unknown modifier before '+'. Try Ctrl, Shift, Alt, or Win.";
            return false;
        }

        keyToken = Trim(text.substr(plus + 1));
        text = keyToken;
        plus = text.find(L'+');
    }

    UCHAR characterModifier = 0;
    if (keyToken.size() == 1 && MapCharacter(keyToken[0], usage, &characterModifier)) {
        *modifier |= characterModifier;
        return true;
    }

    if (LookupNamedKey(keyToken, usage)) {
        return true;
    }

    *error = L"Unknown key. Try A, Ctrl+A, Enter, Space, F1, NumLock, or 0x04.";
    return false;
}

static ControlOpenResult OpenControlDeviceDetailed()
{
    ControlOpenResult result;
    ULONG listLen = 0;
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&listLen, (GUID*)&GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL,
        nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    result.InterfaceListLength = listLen;
    if (cr != CR_SUCCESS) {
        wchar_t buffer[128];
        swprintf_s(buffer, L"CM_Get_Device_Interface_List_SizeW failed: 0x%X", cr);
        result.Message = buffer;
        return result;
    }

    if (listLen <= 1) {
        result.Message = L"No active Spikey control interface found.";
        return result;
    }

    std::wstring list;
    list.resize(listLen);
    cr = CM_Get_Device_Interface_ListW((GUID*)&GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL,
        nullptr, &list[0], listLen, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) {
        wchar_t buffer[128];
        swprintf_s(buffer, L"CM_Get_Device_Interface_ListW failed: 0x%X", cr);
        result.Message = buffer;
        return result;
    }

    DWORD lastError = ERROR_SUCCESS;
    for (wchar_t* path = &list[0]; *path; path += wcslen(path) + 1) {
        result.InterfaceCount++;
        if (result.FirstPath.empty()) {
            result.FirstPath = path;
        }

        HANDLE handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            result.Handle = handle;
            result.Message = L"Opened control interface.";
            return result;
        }

        lastError = GetLastError();
    }

    wchar_t buffer[160];
    swprintf_s(buffer, L"Could not open control interface. Last GetLastError=%lu.", lastError);
    result.LastCreateFileError = lastError;
    result.Message = buffer;
    return result;
}

static HANDLE OpenControlDevice(std::wstring* error)
{
    ControlOpenResult result = OpenControlDeviceDetailed();
    if (result.Handle == INVALID_HANDLE_VALUE) {
        *error = result.Message;
        return INVALID_HANDLE_VALUE;
    }

    return result.Handle;
}

static std::wstring FormatStats(const THESPIKEYDRIVER_STATS_OUTPUT& stats, DWORD bytesReturned)
{
    std::wstring text;
    AppendFormat(&text, L"GET_STATS succeeded, bytesReturned=%lu\r\n", bytesReturned);
    AppendFormat(&text, L"  version=%u size=%u mode=%u sequence=%u\r\n",
        stats.Version, stats.StructSize, stats.Mode, stats.Sequence);
    AppendFormat(&text, L"  state=%s remainingCycles=%u\r\n",
        DescribeCommandState(stats.CommandState), stats.PendingEmitCount);
    AppendFormat(&text, L"  queuedCommands=%u completedReports=%u emptyTicks=%u lastStatus=0x%08X\r\n",
        stats.CommandQueuedCount, stats.ReportsCompletedCount,
        stats.EmptyTicksCount, stats.LastCompletionStatus);
    AppendFormat(&text, L"  pending modifier=0x%02X keycodes=%02X %02X %02X %02X %02X %02X\r\n",
        stats.PendingModifier,
        stats.PendingKeycodes[0], stats.PendingKeycodes[1], stats.PendingKeycodes[2],
        stats.PendingKeycodes[3], stats.PendingKeycodes[4], stats.PendingKeycodes[5]);
    AppendFormat(&text, L"  last report=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
        stats.LastReport[0], stats.LastReport[1], stats.LastReport[2], stats.LastReport[3],
        stats.LastReport[4], stats.LastReport[5], stats.LastReport[6], stats.LastReport[7]);
    return text;
}

static bool TryGetStats(HANDLE control, THESPIKEYDRIVER_STATS_OUTPUT* stats, DWORD* bytesReturned, DWORD* lastError)
{
    ZeroMemory(stats, sizeof(*stats));
    *bytesReturned = 0;
    *lastError = ERROR_SUCCESS;

    BOOL ok = DeviceIoControl(control, IOCTL_THESPIKEYDRIVER_GET_STATS,
        nullptr, 0, stats, sizeof(*stats), bytesReturned, nullptr);
    if (!ok) {
        *lastError = GetLastError();
        return false;
    }

    return true;
}

static bool InjectKeystroke(UCHAR usage, UCHAR modifier, std::wstring* diagnostics)
{
    ControlOpenResult open = OpenControlDeviceDetailed();
    AppendFormat(diagnostics, L"Interface list length=%lu, interfaces=%lu\r\n",
        open.InterfaceListLength, open.InterfaceCount);
    if (!open.FirstPath.empty()) {
        AppendFormat(diagnostics, L"First path: %s\r\n", open.FirstPath.c_str());
    }
    AppendFormat(diagnostics, L"Open: %s\r\n", open.Message.c_str());

    HANDLE control = open.Handle;
    if (control == INVALID_HANDLE_VALUE) {
        return false;
    }

    THESPIKEYDRIVER_INJECT_REPORT_INPUT input = {};
    input.StructSize = sizeof(input);
    input.Version = THESPIKEYDRIVER_CONTROL_VERSION;
    input.Modifier = modifier;
    input.Keycodes[0] = usage;
    input.EmitCount = 1;

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(control, IOCTL_THESPIKEYDRIVER_INJECT_REPORT,
        &input, sizeof(input), nullptr, 0, &bytesReturned, nullptr);
    DWORD lastError = GetLastError();
    AppendFormat(diagnostics, L"INJECT_REPORT input: usage=0x%02X modifier=0x%02X emitCount=%u\r\n",
        usage, modifier, input.EmitCount);
    AppendFormat(diagnostics, L"INJECT_REPORT result: ok=%u bytesReturned=%lu lastError=%lu\r\n",
        ok ? 1 : 0, bytesReturned, ok ? ERROR_SUCCESS : lastError);

    THESPIKEYDRIVER_STATS_OUTPUT stats;
    DWORD statsBytes = 0;
    DWORD statsLastError = ERROR_SUCCESS;
    if (TryGetStats(control, &stats, &statsBytes, &statsLastError)) {
        diagnostics->append(FormatStats(stats, statsBytes));
    } else {
        AppendFormat(diagnostics, L"GET_STATS after inject failed. GetLastError=%lu\r\n", statsLastError);
    }

    CloseHandle(control);

    if (!ok) {
        return false;
    }

    return true;
}

static void RefreshStats()
{
    std::wstring diagnostics;
    ControlOpenResult open = OpenControlDeviceDetailed();
    AppendFormat(&diagnostics, L"Interface list length=%lu, interfaces=%lu\r\n",
        open.InterfaceListLength, open.InterfaceCount);
    if (!open.FirstPath.empty()) {
        AppendFormat(&diagnostics, L"First path: %s\r\n", open.FirstPath.c_str());
    }
    AppendFormat(&diagnostics, L"Open: %s\r\n", open.Message.c_str());

    if (open.Handle == INVALID_HANDLE_VALUE) {
        SetStatus(open.Message.c_str());
        SetDiagnostics(diagnostics);
        return;
    }

    THESPIKEYDRIVER_STATS_OUTPUT stats;
    DWORD bytesReturned = 0;
    DWORD lastError = ERROR_SUCCESS;
    if (TryGetStats(open.Handle, &stats, &bytesReturned, &lastError)) {
        diagnostics.append(FormatStats(stats, bytesReturned));
        SetStatus(L"Stats refreshed.");
    } else {
        AppendFormat(&diagnostics, L"GET_STATS failed. GetLastError=%lu\r\n", lastError);
        SetStatus(L"GET_STATS failed.");
    }

    CloseHandle(open.Handle);
    SetDiagnostics(diagnostics);
}

static void StopCountdown(HWND hwnd)
{
    KillTimer(hwnd, COUNTDOWN_TIMER_ID);
    g_SecondsRemaining = 0;
    EnableWindow(g_StartButton, TRUE);
    EnableWindow(g_CancelButton, FALSE);
}

static void StartCountdown(HWND hwnd)
{
    wchar_t text[128];
    GetWindowTextW(g_KeyEdit, text, ARRAYSIZE(text));

    std::wstring error;
    if (!ParseKeystroke(text, &g_PendingUsage, &g_PendingModifier, &error)) {
        SetStatus(error.c_str());
        SetDiagnostics(error);
        return;
    }

    g_SecondsRemaining = 10;
    EnableWindow(g_StartButton, FALSE);
    EnableWindow(g_CancelButton, TRUE);

    wchar_t status[160];
    swprintf_s(status, L"Injecting usage 0x%02X modifier 0x%02X in 10 seconds...", g_PendingUsage, g_PendingModifier);
    SetStatus(status);
    SetTimer(hwnd, COUNTDOWN_TIMER_ID, 1000, nullptr);
}

static void OnCountdownTick(HWND hwnd)
{
    if (g_SecondsRemaining > 0) {
        g_SecondsRemaining--;
    }

    if (g_SecondsRemaining > 0) {
        wchar_t status[160];
        swprintf_s(status, L"Injecting usage 0x%02X modifier 0x%02X in %d seconds...",
            g_PendingUsage, g_PendingModifier, g_SecondsRemaining);
        SetStatus(status);
        return;
    }

    StopCountdown(hwnd);

    std::wstring diagnostics;
    if (!InjectKeystroke(g_PendingUsage, g_PendingModifier, &diagnostics)) {
        SetStatus(L"Injection failed. See diagnostics.");
        SetDiagnostics(diagnostics);
        return;
    }

    wchar_t status[160];
    swprintf_s(status, L"Injected usage 0x%02X modifier 0x%02X.", g_PendingUsage, g_PendingModifier);
    SetStatus(status);
    SetDiagnostics(diagnostics);
}

static HWND AddControl(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style,
    int x, int y, int width, int height, int id)
{
    return CreateWindowExW(0, className, text, style | WS_CHILD | WS_VISIBLE,
        x, y, width, height, parent, (HMENU)(INT_PTR)id, g_Instance, nullptr);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        AddControl(hwnd, L"STATIC", L"Keystroke", WS_TABSTOP, 18, 18, 90, 22, -1);
        g_KeyEdit = AddControl(hwnd, L"EDIT", L"a", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            110, 14, 170, 26, IDC_KEY_EDIT);
        g_StartButton = AddControl(hwnd, L"BUTTON", L"Start 10s Countdown", BS_PUSHBUTTON | WS_TABSTOP,
            292, 13, 160, 28, IDC_START_BUTTON);
        g_CancelButton = AddControl(hwnd, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP,
            462, 13, 80, 28, IDC_CANCEL_BUTTON);
        EnableWindow(g_CancelButton, FALSE);
        g_StatsButton = AddControl(hwnd, L"BUTTON", L"Refresh Stats", BS_PUSHBUTTON | WS_TABSTOP,
            18, 84, 120, 28, IDC_STATS_BUTTON);

        AddControl(hwnd, L"STATIC",
            L"Examples: a, A, Ctrl+A, Enter, Space, F1, NumLock, 0x04",
            0, 18, 54, 524, 22, IDC_HELP_TEXT);
        g_StatusText = AddControl(hwnd, L"STATIC", L"Ready.", 0, 150, 86, 392, 44, IDC_STATUS_TEXT);
        g_DiagnosticsEdit = AddControl(hwnd, L"EDIT", L"Diagnostics will appear here.",
            WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            18, 124, 524, 190, IDC_DIAGNOSTICS_EDIT);
        SendMessageW(g_KeyEdit, EM_SETLIMITTEXT, 64, 0);
        SetFocus(g_KeyEdit);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START_BUTTON:
            StartCountdown(hwnd);
            return 0;
        case IDC_CANCEL_BUTTON:
            StopCountdown(hwnd);
            SetStatus(L"Cancelled.");
            return 0;
        case IDC_STATS_BUTTON:
            RefreshStats();
            return 0;
        default:
            break;
        }
        break;

    case WM_TIMER:
        if (wParam == COUNTDOWN_TIMER_ID) {
            OnCountdownTick(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, COUNTDOWN_TIMER_ID);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    g_Instance = instance;

    const wchar_t className[] = L"SpikeyKeystrokeInjectorWindow";

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;

    if (!RegisterClassW(&windowClass)) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, className, L"Spikey Keystroke Injector",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 370,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
