#include <windows.h>
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdio.h>
#include <vector>
#include <string>

#include "driverspike-common.h"
#include "driverspike-joystick-common.h"

static void PrintWin32Error(const char* what) { fprintf(stderr, "%s failed, GetLastError=%lu\n", what, GetLastError()); }

static bool ParseByteArg(const wchar_t* text, UCHAR* value)
{
    wchar_t* end = nullptr;
    unsigned long parsed = wcstoul(text, &end, 0);
    if (end == text || *end != L'\0' || parsed > 0xFF) return false;
    *value = (UCHAR)parsed;
    return true;
}

static bool ParseUIntArg(const wchar_t* text, unsigned int* value)
{
    wchar_t* end = nullptr;
    unsigned long parsed = wcstoul(text, &end, 0);
    if (end == text || *end != L'\0' || parsed > 0xFFFFFFFFul) return false;
    *value = (unsigned int)parsed;
    return true;
}

static bool ParseIntArg(const wchar_t* text, int* value)
{
    wchar_t* end = nullptr;
    long parsed = wcstol(text, &end, 0);
    if (end == text || *end != L'\0') return false;
    *value = (int)parsed;
    return true;
}

static HANDLE TryOpenHidPath(_In_z_ const wchar_t* path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) return h;
    return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
}

static bool QueryHidIdentity(HANDLE file, HIDD_ATTRIBUTES* attributes, HIDP_CAPS* caps)
{
    PHIDP_PREPARSED_DATA ppd = nullptr;
    ZeroMemory(attributes, sizeof(*attributes));
    attributes->Size = sizeof(*attributes);
    if (!HidD_GetAttributes(file, attributes)) return false;
    if (!HidD_GetPreparsedData(file, &ppd)) return false;
    ZeroMemory(caps, sizeof(*caps));
    if (HidP_GetCaps(ppd, caps) != HIDP_STATUS_SUCCESS) {
        HidD_FreePreparsedData(ppd);
        SetLastError(ERROR_GEN_FAILURE);
        return false;
    }
    HidD_FreePreparsedData(ppd);
    return true;
}

static bool IsSCMFDKeyboardControlCollection(const HIDP_CAPS* caps)
{
    return (caps->UsagePage == CONTROL_COLLECTION_USAGE_PAGE &&
        caps->Usage == CONTROL_COLLECTION_USAGE &&
        caps->FeatureReportByteLength == SCMFD_KEYBOARD_ROOT_FEATURE_REPORT_SIZE_CB);
}

static bool IsSCMFDJoystickControlCollection(const HIDP_CAPS* caps)
{
    return (caps->UsagePage == JOYSTICK_CONTROL_COLLECTION_USAGE_PAGE &&
        caps->Usage == JOYSTICK_CONTROL_COLLECTION_USAGE &&
        caps->FeatureReportByteLength == SCMFD_JOYSTICK_ROOT_FEATURE_REPORT_SIZE_CB);
}

static bool IsSCMFDJoystickCollection(const HIDD_ATTRIBUTES* attr, const HIDP_CAPS* caps)
{
    return (attr->VendorID == SCMFD_JOYSTICK_ROOT_VID &&
        attr->ProductID == SCMFD_JOYSTICK_ROOT_PID &&
        caps->UsagePage == 0x0001 &&
        caps->Usage == 0x0004 &&
        caps->InputReportByteLength == SCMFD_JOYSTICK_ROOT_INPUT_REPORT_SIZE_CB);
}

static bool ListAllHidInterfaces(void)
{
    GUID hidGuid;
    ULONG len = 0;
    HidD_GetHidGuid(&hidGuid);
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&len, &hidGuid, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) return false;
    if (len <= 1) return true;

    std::vector<wchar_t> list(len);
    cr = CM_Get_Device_Interface_ListW(&hidGuid, nullptr, list.data(), len, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) return false;

    for (wchar_t* p = list.data(); *p; p += wcslen(p) + 1) {
        HANDLE h = TryOpenHidPath(p);
        if (h == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attr = {};
        HIDP_CAPS caps = {};
        if (QueryHidIdentity(h, &attr, &caps)) {
            const wchar_t* tag = L"";
            if (IsSCMFDKeyboardControlCollection(&caps)) tag = L" [scmfd-keyboard-control]";
            else if (IsSCMFDJoystickControlCollection(&caps)) tag = L" [scmfd-joystick-control]";
            else if (IsSCMFDJoystickCollection(&attr, &caps)) tag = L" [scmfd-joystick]";

            wprintf(L"path=%s VID=0x%04X PID=0x%04X UsagePage=0x%04X Usage=0x%04X InputBytes=%u OutputBytes=%u FeatureBytes=%u%s\n",
                p, attr.VendorID, attr.ProductID, caps.UsagePage, caps.Usage,
                caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, tag);
        }
        CloseHandle(h);
    }
    return true;
}

static bool FindControlDevice(HANDLE* outHandle)
{
    GUID hidGuid;
    ULONG listLen = 0;
    std::vector<std::wstring> candidates;
    std::vector<std::wstring> allSCMFDKeyboard;

    HidD_GetHidGuid(&hidGuid);
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&listLen, &hidGuid, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS || listLen <= 1) return false;

    std::vector<wchar_t> list(listLen);
    cr = CM_Get_Device_Interface_ListW(&hidGuid, nullptr, list.data(), listLen, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) return false;

    for (wchar_t* cur = list.data(); *cur; cur += wcslen(cur) + 1) {
        HANDLE h = TryOpenHidPath(cur);
        if (h == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attr = {};
        HIDP_CAPS caps = {};
        bool ok = QueryHidIdentity(h, &attr, &caps);
        CloseHandle(h);
        if (!ok || !IsSCMFDKeyboardControlCollection(&caps)) continue;
        allSCMFDKeyboard.push_back(cur);
        if (attr.VendorID == 0xDEED && attr.ProductID == 0xFEED) candidates.push_back(cur);
    }

    const std::vector<std::wstring>& target = !candidates.empty() ? candidates : allSCMFDKeyboard;
    if (target.empty()) {
        fprintf(stderr, "No SCMFD Keyboard control collection found.\n");
        return false;
    }
    if (target.size() > 1) {
        fprintf(stderr, "ERROR: duplicate SCMFD Keyboard control collections found (%zu). Clean stale SCMFD Keyboard devices/packages.\n", target.size());
        for (const std::wstring& path : target) wprintf(L"  candidate: %s\n", path.c_str());
        return false;
    }

    HANDLE h = TryOpenHidPath(target[0].c_str());
    if (h == INVALID_HANDLE_VALUE) {
        PrintWin32Error("CreateFile(control)");
        return false;
    }
    *outHandle = h;
    return true;
}

static bool FindJoystickControlDevice(HANDLE* outHandle)
{
    GUID hidGuid;
    ULONG listLen = 0;
    std::vector<std::wstring> candidates;

    HidD_GetHidGuid(&hidGuid);
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&listLen, &hidGuid, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS || listLen <= 1) return false;

    std::vector<wchar_t> list(listLen);
    cr = CM_Get_Device_Interface_ListW(&hidGuid, nullptr, list.data(), listLen, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) return false;

    for (wchar_t* cur = list.data(); *cur; cur += wcslen(cur) + 1) {
        HANDLE h = TryOpenHidPath(cur);
        if (h == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attr = {};
        HIDP_CAPS caps = {};
        bool ok = QueryHidIdentity(h, &attr, &caps);
        CloseHandle(h);
        if (!ok || !IsSCMFDJoystickControlCollection(&caps)) continue;
        if (attr.VendorID == SCMFD_JOYSTICK_ROOT_VID && attr.ProductID == SCMFD_JOYSTICK_ROOT_PID) {
            candidates.push_back(cur);
        }
    }

    if (candidates.empty()) {
        fprintf(stderr, "No SCMFD Joystick control collection found.\n");
        return false;
    }
    if (candidates.size() > 1) {
        fprintf(stderr, "ERROR: duplicate SCMFD Joystick control collections found (%zu). Clean stale SCMFD Joystick devices/packages.\n", candidates.size());
        for (const std::wstring& path : candidates) wprintf(L"  candidate: %s\n", path.c_str());
        return false;
    }

    HANDLE h = TryOpenHidPath(candidates[0].c_str());
    if (h == INVALID_HANDLE_VALUE) {
        PrintWin32Error("CreateFile(joystick-control)");
        return false;
    }
    *outHandle = h;
    return true;
}

static bool SendKeyEvent(HANDLE file, UCHAR controlCode, UCHAR usage, ULONG maxHoldMs)
{
    SCMFD_KEYBOARD_ROOT_FEATURE_REPORT packet = {};
    packet.ReportId = CONTROL_COLLECTION_REPORT_ID;
    packet.ControlCode = controlCode;
    packet.Version = SCMFD_KEYBOARD_ROOT_FEATURE_VERSION;
    packet.Payload.KeyEvent.StructSize = sizeof(packet.Payload.KeyEvent);
    packet.Payload.KeyEvent.Version = SCMFD_KEYBOARD_ROOT_CONTROL_VERSION;
    packet.Payload.KeyEvent.Usage = usage;
    packet.Payload.KeyEvent.MaxHoldMs = maxHoldMs;
    if (!HidD_SetFeature(file, &packet, sizeof(packet))) {
        PrintWin32Error("HidD_SetFeature(key-event)");
        return false;
    }
    return true;
}

static bool SendReleaseAll(HANDLE file)
{
    SCMFD_KEYBOARD_ROOT_FEATURE_REPORT packet = {};
    packet.ReportId = CONTROL_COLLECTION_REPORT_ID;
    packet.ControlCode = SCMFD_KEYBOARD_ROOT_FEATURE_RELEASE_ALL;
    packet.Version = SCMFD_KEYBOARD_ROOT_FEATURE_VERSION;
    if (!HidD_SetFeature(file, &packet, sizeof(packet))) {
        PrintWin32Error("HidD_SetFeature(release-all)");
        return false;
    }
    return true;
}

static bool GetDriverStats(HANDLE file)
{
    SCMFD_KEYBOARD_ROOT_FEATURE_REPORT packet = {};
    packet.ReportId = CONTROL_COLLECTION_REPORT_ID;
    packet.ControlCode = SCMFD_KEYBOARD_ROOT_FEATURE_GET_STATS;
    packet.Version = SCMFD_KEYBOARD_ROOT_FEATURE_VERSION;
    if (!HidD_GetFeature(file, &packet, sizeof(packet))) {
        PrintWin32Error("HidD_GetFeature(GET_STATS)");
        return false;
    }
    const SCMFD_KEYBOARD_ROOT_STATS_OUTPUT& s = packet.Payload.Stats;
    printf("Stats seq=%u down=%u up=%u releaseAll=%u autoRelease=%u dupNoOp=%u rolloverReject=%u completed=%u lastCmdStatus=%u lastCmdUsage=0x%02X currentModifier=0x%02X currentKeys=%02X %02X %02X %02X %02X %02X\n",
        s.Sequence, s.KeyDownCount, s.KeyUpCount, s.ReleaseAllCount, s.AutoReleaseCount, s.DuplicateNoOpCount, s.RolloverRejectCount,
        s.ReportsCompletedCount, s.LastCommandStatus, s.LastCommandUsage, s.CurrentModifier,
        s.CurrentKeycodes[0], s.CurrentKeycodes[1], s.CurrentKeycodes[2], s.CurrentKeycodes[3], s.CurrentKeycodes[4], s.CurrentKeycodes[5]);
    return true;
}

static bool SendJoystickButton(HANDLE file, unsigned int button, bool pressed)
{
    SCMFD_JOYSTICK_ROOT_FEATURE_REPORT packet = {};
    packet.ReportId = JOYSTICK_CONTROL_COLLECTION_REPORT_ID;
    packet.ControlCode = SCMFD_JOYSTICK_ROOT_FEATURE_BUTTON;
    packet.Version = SCMFD_JOYSTICK_ROOT_FEATURE_VERSION;
    packet.Payload.Button.StructSize = sizeof(packet.Payload.Button);
    packet.Payload.Button.Version = SCMFD_JOYSTICK_ROOT_CONTROL_VERSION;
    packet.Payload.Button.Button = button;
    packet.Payload.Button.Pressed = pressed ? 1u : 0u;
    if (!HidD_SetFeature(file, &packet, sizeof(packet))) {
        PrintWin32Error("HidD_SetFeature(joy-button)");
        return false;
    }
    return true;
}

static bool SendJoystickAxis(HANDLE file, unsigned int axis, int value)
{
    SCMFD_JOYSTICK_ROOT_FEATURE_REPORT packet = {};
    packet.ReportId = JOYSTICK_CONTROL_COLLECTION_REPORT_ID;
    packet.ControlCode = SCMFD_JOYSTICK_ROOT_FEATURE_AXIS;
    packet.Version = SCMFD_JOYSTICK_ROOT_FEATURE_VERSION;
    packet.Payload.Axis.StructSize = sizeof(packet.Payload.Axis);
    packet.Payload.Axis.Version = SCMFD_JOYSTICK_ROOT_CONTROL_VERSION;
    packet.Payload.Axis.Axis = axis;
    packet.Payload.Axis.Value = value;
    if (!HidD_SetFeature(file, &packet, sizeof(packet))) {
        PrintWin32Error("HidD_SetFeature(joy-axis)");
        return false;
    }
    return true;
}

static bool SendJoystickReleaseAll(HANDLE file)
{
    SCMFD_JOYSTICK_ROOT_FEATURE_REPORT packet = {};
    packet.ReportId = JOYSTICK_CONTROL_COLLECTION_REPORT_ID;
    packet.ControlCode = SCMFD_JOYSTICK_ROOT_FEATURE_RELEASE_ALL;
    packet.Version = SCMFD_JOYSTICK_ROOT_FEATURE_VERSION;
    if (!HidD_SetFeature(file, &packet, sizeof(packet))) {
        PrintWin32Error("HidD_SetFeature(joy-release-all)");
        return false;
    }
    return true;
}

static bool GetJoystickStats(HANDLE file)
{
    SCMFD_JOYSTICK_ROOT_FEATURE_REPORT packet = {};
    packet.ReportId = JOYSTICK_CONTROL_COLLECTION_REPORT_ID;
    packet.ControlCode = SCMFD_JOYSTICK_ROOT_FEATURE_GET_STATS;
    packet.Version = SCMFD_JOYSTICK_ROOT_FEATURE_VERSION;
    if (!HidD_GetFeature(file, &packet, sizeof(packet))) {
        PrintWin32Error("HidD_GetFeature(JOY_GET_STATS)");
        return false;
    }

    const SCMFD_JOYSTICK_ROOT_STATS_OUTPUT& s = packet.Payload.Stats;
    printf("JoyStats seq=%u buttons=%u axes=%u releaseAll=%u completed=%u empty=%u lastCompletion=0x%08X lastCmd=%u lastButton=%u lastAxis=%u\n",
        s.Sequence, s.ButtonCommandCount, s.AxisCommandCount, s.ReleaseAllCount,
        s.ReportsCompletedCount, s.EmptyTicksCount, s.LastCompletionStatus,
        s.LastCommandStatus, s.LastButton, s.LastAxis);
    printf("Current axes: X=%d Y=%d Z=%d Rx=%d Ry=%d Rz=%d Slider=%d Dial=%d\n",
        s.CurrentReport.Axes[0], s.CurrentReport.Axes[1], s.CurrentReport.Axes[2], s.CurrentReport.Axes[3],
        s.CurrentReport.Axes[4], s.CurrentReport.Axes[5], s.CurrentReport.Axes[6], s.CurrentReport.Axes[7]);
    printf("Current buttons bytes: %02X %02X %02X %02X ... %02X %02X %02X %02X\n",
        s.CurrentReport.Buttons[0], s.CurrentReport.Buttons[1], s.CurrentReport.Buttons[2], s.CurrentReport.Buttons[3],
        s.CurrentReport.Buttons[12], s.CurrentReport.Buttons[13], s.CurrentReport.Buttons[14], s.CurrentReport.Buttons[15]);
    return true;
}

static bool ParseAxisName(const wchar_t* text, unsigned int* axis)
{
    if (_wcsicmp(text, L"x") == 0) { *axis = 0; return true; }
    if (_wcsicmp(text, L"y") == 0) { *axis = 1; return true; }
    if (_wcsicmp(text, L"z") == 0) { *axis = 2; return true; }
    if (_wcsicmp(text, L"rx") == 0) { *axis = 3; return true; }
    if (_wcsicmp(text, L"ry") == 0) { *axis = 4; return true; }
    if (_wcsicmp(text, L"rz") == 0) { *axis = 5; return true; }
    if (_wcsicmp(text, L"slider") == 0 || _wcsicmp(text, L"s0") == 0) { *axis = 6; return true; }
    if (_wcsicmp(text, L"dial") == 0 || _wcsicmp(text, L"s1") == 0) { *axis = 7; return true; }
    return ParseUIntArg(text, axis) && *axis < SCMFD_JOYSTICK_ROOT_AXIS_COUNT;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc > 1 && wcscmp(argv[1], L"--list-hid") == 0) return ListAllHidInterfaces() ? 0 : 1;

    if (argc > 1 && wcsncmp(argv[1], L"--joy", 5) == 0) {
        HANDLE joyControl = INVALID_HANDLE_VALUE;
        if (!FindJoystickControlDevice(&joyControl)) return 1;

        bool joyOk = false;
        if (wcscmp(argv[1], L"--joy-stats") == 0) {
            joyOk = GetJoystickStats(joyControl);
        } else if (argc > 3 && wcscmp(argv[1], L"--joy-button") == 0) {
            unsigned int button = 0;
            bool pressed = (_wcsicmp(argv[3], L"down") == 0 || _wcsicmp(argv[3], L"on") == 0 || wcscmp(argv[3], L"1") == 0);
            bool released = (_wcsicmp(argv[3], L"up") == 0 || _wcsicmp(argv[3], L"off") == 0 || wcscmp(argv[3], L"0") == 0);
            joyOk = ParseUIntArg(argv[2], &button) && (pressed || released) && SendJoystickButton(joyControl, button, pressed);
        } else if (argc > 3 && wcscmp(argv[1], L"--joy-axis") == 0) {
            unsigned int axis = 0;
            int value = 0;
            joyOk = ParseAxisName(argv[2], &axis) && ParseIntArg(argv[3], &value) && SendJoystickAxis(joyControl, axis, value);
        } else if (wcscmp(argv[1], L"--joy-release-all") == 0) {
            joyOk = SendJoystickReleaseAll(joyControl);
        } else {
            fprintf(stderr, "Usage: --joy-stats | --joy-button <1..128> down|up | --joy-axis x|y|z|rx|ry|rz|slider|dial <value> | --joy-release-all\n");
        }

        CloseHandle(joyControl);
        return joyOk ? 0 : 1;
    }

    HANDLE control = INVALID_HANDLE_VALUE;
    if (!FindControlDevice(&control)) return 1;

    bool ok = false;
    if (argc > 1 && wcscmp(argv[1], L"--stats") == 0) {
        ok = GetDriverStats(control);
    } else if (argc > 2 && wcscmp(argv[1], L"--down") == 0) {
        UCHAR usage = 0; ok = ParseByteArg(argv[2], &usage) && SendKeyEvent(control, SCMFD_KEYBOARD_ROOT_FEATURE_KEY_DOWN, usage, 0);
    } else if (argc > 2 && wcscmp(argv[1], L"--up") == 0) {
        UCHAR usage = 0; ok = ParseByteArg(argv[2], &usage) && SendKeyEvent(control, SCMFD_KEYBOARD_ROOT_FEATURE_KEY_UP, usage, 0);
    } else if (argc > 2 && wcscmp(argv[1], L"--tap") == 0) {
        UCHAR usage = 0; ULONG durationMs = (argc > 3) ? wcstoul(argv[3], nullptr, 0) : 50;
        ok = ParseByteArg(argv[2], &usage) &&
            SendKeyEvent(control, SCMFD_KEYBOARD_ROOT_FEATURE_KEY_DOWN, usage, 0) &&
            (Sleep(durationMs), true) &&
            SendKeyEvent(control, SCMFD_KEYBOARD_ROOT_FEATURE_KEY_UP, usage, 0);
    } else if (argc > 1 && wcscmp(argv[1], L"--release-all") == 0) {
        ok = SendReleaseAll(control);
    } else if (argc > 2 && wcscmp(argv[1], L"--inject") == 0) {
        UCHAR usage = 0;
        ULONG durationMs = (argc > 3) ? wcstoul(argv[3], nullptr, 0) : 50;
        fprintf(stderr, "--inject is legacy; use --tap. Running legacy alias now.\n");
        ok = ParseByteArg(argv[2], &usage) &&
            SendKeyEvent(control, SCMFD_KEYBOARD_ROOT_FEATURE_KEY_DOWN, usage, 0) &&
            (Sleep(durationMs), true) &&
            SendKeyEvent(control, SCMFD_KEYBOARD_ROOT_FEATURE_KEY_UP, usage, 0);
    } else {
        fprintf(stderr, "Usage: --list-hid | --stats | --down 0xNN | --up 0xNN | --tap 0xNN [durationMs] | --release-all | --inject 0xNN [durationMs]\n");
    }

    CloseHandle(control);
    return ok ? 0 : 1;
}
