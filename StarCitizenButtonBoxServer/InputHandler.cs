using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using WindowsInput.Native;

namespace StarCitizenButtonBoxServer;

public enum MouseInputAction
{
    None,
    LeftClick,
    MiddleClick,
    RightClick,
    ScrollUp,
    ScrollDown
}

// Routes keyboard input through the Interception kernel filter driver, which bypasses
// the injected-input flag that Star Citizen uses to ignore normal Win32 SendInput calls.
// Prerequisite: install ThirdParty/Interception/install-interception.exe /install as admin.
public sealed class InputHandler : IDisposable
{
    readonly IntPtr _ctx;
    readonly Action<string>? _log;
    readonly List<int> _keyboardDevices;

    // Interception device indices: Keyboard (1-10), Mouse (11-20).
    int _keyboardDevice = 1;

    public InputHandler(Action<string>? log = null)
    {
        _log = log;
        _ctx = interception_create_context();
        if (_ctx == IntPtr.Zero)
            throw new InvalidOperationException(
                "Interception context creation failed. " +
                "Ensure the Interception driver is installed (run install-interception.exe /install as admin).");

        _keyboardDevices = DiscoverKeyboards();
        if (_keyboardDevices.Count == 0)
            throw new InvalidOperationException(
                "Interception did not report any keyboard devices. Plug in a keyboard and restart.");

        _keyboardDevice = _keyboardDevices[0];
        _log?.Invoke($"[Interception] Initialized. Using keyboard device {_keyboardDevice}.");
    }

    List<int> DiscoverKeyboards()
    {
        var devices = new List<int>();

        for (int i = 1; i <= 10; i++)
        {
            if (interception_is_keyboard(i) == 0)
                continue;

            var hardwareId = new StringBuilder(512);
            var length = interception_get_hardware_id(_ctx, i, hardwareId, hardwareId.Capacity);
            if (length <= 0)
                continue;

            devices.Add(i);
            _log?.Invoke($"[Interception] Found keyboard device {i}: {hardwareId}");
        }

        return devices;
    }

    public async Task ExecuteAsync(BindingEntry binding, CancellationToken cancellationToken = default)
    {
        var hasMouse     = binding.MouseInputAction != MouseInputAction.None;
        var hasKey       = !string.IsNullOrWhiteSpace(binding.Key);
        var hasModifiers = binding.LeftCtrl || binding.RightCtrl || binding.LeftShift ||
                           binding.RightShift || binding.LeftAlt || binding.RightAlt;

        if (!hasMouse && !hasKey && !hasModifiers) return;
        if (hasKey && !hasMouse && !VirtualKeyHelper.TryResolveVirtualKey(binding.Key!, out _)) return;

        var hold = Math.Clamp(binding.HoldTimeMs, 1, 1000);

        // Modifier scan codes don't vary by keyboard layout, so they're hardcoded here.
        if (binding.LeftCtrl)   SendKey(0x1D, down: true);
        if (binding.RightCtrl)  SendKey(0x1D, down: true,  extended: true);
        if (binding.LeftShift)  SendKey(0x2A, down: true);
        if (binding.RightShift) SendKey(0x36, down: true);
        if (binding.LeftAlt)    SendKey(0x38, down: true);
        if (binding.RightAlt)   SendKey(0x38, down: true,  extended: true);

        await Task.Delay(5, cancellationToken);

        if (hasMouse)
        {
            MouseDown(binding.MouseInputAction);
            await Task.Delay(hold, cancellationToken);
            MouseUp(binding.MouseInputAction);
        }
        else if (hasKey)
        {
            VirtualKeyHelper.TryResolveVirtualKey(binding.Key!, out var vk);
            var (scan, ext) = VkToScan(vk);
            SendKey(scan, down: true,  extended: ext);
            await Task.Delay(hold, cancellationToken);
            SendKey(scan, down: false, extended: ext);
        }
        else
        {
            await Task.Delay(hold, cancellationToken);
        }

        await Task.Delay(5, cancellationToken);

        // Release backwards so combos unwind like a real keyboard hand.
        if (binding.RightAlt)   SendKey(0x38, down: false, extended: true);
        if (binding.LeftAlt)    SendKey(0x38, down: false);
        if (binding.RightShift) SendKey(0x36, down: false);
        if (binding.LeftShift)  SendKey(0x2A, down: false);
        if (binding.RightCtrl)  SendKey(0x1D, down: false, extended: true);
        if (binding.LeftCtrl)   SendKey(0x1D, down: false);
    }

    void SendKey(ushort scanCode, bool down, bool extended = false)
    {
        ushort state = (ushort)(down ? KEY_DOWN : KEY_UP);
        if (extended) state |= KEY_E0;
        var stroke = new InterceptionKeyStroke { code = scanCode, state = state };

        var sent = SendKeyToCurrentDevice(ref stroke);
        if (sent == 0)
            sent = TryOtherKeyboards(ref stroke);

        _log?.Invoke(
            $"[Interception] dev={_keyboardDevice} scan=0x{scanCode:X2}{(extended ? " E0" : "")} {(down ? "DOWN" : "UP  ")} sent={sent}{(sent == 0 ? " (FAILED)" : "")}");
    }

    int SendKeyToCurrentDevice(ref InterceptionKeyStroke stroke)
    {
        return interception_send(_ctx, _keyboardDevice, ref stroke, 1);
    }

    int TryOtherKeyboards(ref InterceptionKeyStroke stroke)
    {
        foreach (var device in _keyboardDevices)
        {
            if (device == _keyboardDevice)
                continue;

            var sent = interception_send(_ctx, device, ref stroke, 1);
            if (sent == 1)
            {
                _log?.Invoke($"[Interception] Switched keyboard device {_keyboardDevice} -> {device} after a failed send.");
                _keyboardDevice = device;
                return sent;
            }
        }

        return 0;
    }

    // Star Citizen only gets picky about the keyboard. Mouse input can stay boring.
    void MouseDown(MouseInputAction action)
    {
        _log?.Invoke($"[Interception] Mouse {action} DOWN (via Win32 SendInput)");
        switch (action)
        {
            case MouseInputAction.LeftClick:   SendMouseInput(MOUSEEVENTF_LEFTDOWN, 0); break;
            case MouseInputAction.RightClick:  SendMouseInput(MOUSEEVENTF_RIGHTDOWN, 0); break;
            case MouseInputAction.MiddleClick: SendMouseInput(MOUSEEVENTF_MIDDLEDOWN, 0); break;
            case MouseInputAction.ScrollUp:    SendMouseInput(MOUSEEVENTF_WHEEL, 120); break;
            case MouseInputAction.ScrollDown:  SendMouseInput(MOUSEEVENTF_WHEEL, unchecked((int)0xFFFFFF88)); break; // -120, but Win32 wanted drama.
        }
    }

    void MouseUp(MouseInputAction action)
    {
        switch (action)
        {
            case MouseInputAction.LeftClick:   SendMouseInput(MOUSEEVENTF_LEFTUP, 0); break;
            case MouseInputAction.RightClick:  SendMouseInput(MOUSEEVENTF_RIGHTUP, 0); break;
            case MouseInputAction.MiddleClick: SendMouseInput(MOUSEEVENTF_MIDDLEUP, 0); break;
            // Scroll wheels do not get key-up events. Blessedly, one less thing.
        }
    }

    static void SendMouseInput(uint flags, int data)
    {
        var input = new MOUSE_INPUT
        {
            type = INPUT_MOUSE,
            mi   = new MOUSEINPUT { dwFlags = flags, mouseData = data }
        };
        SendInput(1, ref input, Marshal.SizeOf<MOUSE_INPUT>());
    }

    // Maps a VirtualKeyCode to its hardware scan code and whether it needs the E0 extended prefix.
    static (ushort scan, bool extended) VkToScan(VirtualKeyCode vk)
    {
        uint scan = MapVirtualKey((uint)vk, MAPVK_VK_TO_VSC);
        bool extended = s_extendedVks.Contains((uint)vk);
        return ((ushort)scan, extended);
    }

    // These VKs all have E0-prefixed scan codes on real hardware.
    static readonly HashSet<uint> s_extendedVks = new()
    {
        0x21, // VK_PRIOR      Page Up
        0x22, // VK_NEXT       Page Down
        0x23, // VK_END
        0x24, // VK_HOME
        0x25, // VK_LEFT
        0x26, // VK_UP
        0x27, // VK_RIGHT
        0x28, // VK_DOWN
        0x2D, // VK_INSERT
        0x2E, // VK_DELETE
        0x5B, // VK_LWIN
        0x5C, // VK_RWIN
        0x5D, // VK_APPS
        0x6F, // VK_DIVIDE     NumPad /
        0x90, // VK_NUMLOCK
        0xA3, // VK_RCONTROL
        0xA5, // VK_RMENU      Right Alt
    };

    public void Dispose()
    {
        if (_ctx != IntPtr.Zero)
            interception_destroy_context(_ctx);
    }

    // Interception P/Invoke
    const ushort KEY_DOWN = 0x00;
    const ushort KEY_UP   = 0x01;
    const ushort KEY_E0   = 0x02; // INTERCEPTION_KEY_E0

    [DllImport("interception.dll")] static extern IntPtr interception_create_context();
    [DllImport("interception.dll")] static extern void   interception_destroy_context(IntPtr context);
    [DllImport("interception.dll")] static extern int    interception_send(IntPtr context, int device, ref InterceptionKeyStroke stroke, uint nstroke);
    [DllImport("interception.dll")] static extern int    interception_is_keyboard(int device);
    [DllImport("interception.dll", CharSet = CharSet.Unicode)]
    static extern int interception_get_hardware_id(IntPtr context, int device, StringBuilder hardwareIdBuffer,
        int bufferSize);

    [StructLayout(LayoutKind.Sequential)]
    struct InterceptionKeyStroke
    {
        public ushort code;
        public ushort state;
        public uint   information;
    }

    // Win32 mouse SendInput P/Invoke
    const uint INPUT_MOUSE           = 0;
    const uint MOUSEEVENTF_LEFTDOWN   = 0x0002;
    const uint MOUSEEVENTF_LEFTUP     = 0x0004;
    const uint MOUSEEVENTF_RIGHTDOWN  = 0x0008;
    const uint MOUSEEVENTF_RIGHTUP    = 0x0010;
    const uint MOUSEEVENTF_MIDDLEDOWN = 0x0020;
    const uint MOUSEEVENTF_MIDDLEUP   = 0x0040;
    const uint MOUSEEVENTF_WHEEL      = 0x0800;

    const uint MAPVK_VK_TO_VSC = 0;

    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint uCode, uint uMapType);
    [DllImport("user32.dll")] static extern uint SendInput(uint nInputs, ref MOUSE_INPUT pInputs, int cbSize);

    [StructLayout(LayoutKind.Sequential)]
    struct MOUSE_INPUT { public uint type; public MOUSEINPUT mi; }

    [StructLayout(LayoutKind.Sequential)]
    struct MOUSEINPUT
    {
        public int  dx, dy;
        public int  mouseData;
        public uint dwFlags;
        public uint time;
        public IntPtr dwExtraInfo;
    }
}

public static class VirtualKeyHelper
{
    public static bool TryResolveVirtualKey(Key wpfKey, out VirtualKeyCode vk)
    {
        var n = KeyInterop.VirtualKeyFromKey(wpfKey);
        vk = (VirtualKeyCode)n;
        return n != 0;
    }

    public static bool TryResolveVirtualKey(string key, out VirtualKeyCode vk)
    {
        vk = default;
        var s = key.Trim();
        if (s.Length == 0) return false;

        if (Enum.TryParse<VirtualKeyCode>(s, true, out vk))
            return true;

        if (s.Length == 1)
        {
            var c = s[0];
            if (c >= 'a' && c <= 'z') c = char.ToUpperInvariant(c);
            if (c >= 'A' && c <= 'Z')
            {
                vk = VirtualKeyCode.VK_A + (c - 'A');
                return true;
            }
            if (c >= '0' && c <= '9')
            {
                vk = VirtualKeyCode.VK_0 + (c - '0');
                return true;
            }
        }

        var normalized = Regex.Replace(s, @"[\s\-]", "");
        normalized = normalized.ToUpperInvariant();
        var withVk = normalized.StartsWith("VK_", StringComparison.Ordinal) ? normalized : "VK_" + normalized;
        if (Enum.TryParse<VirtualKeyCode>(withVk, true, out vk))
            return true;

        return Enum.TryParse<VirtualKeyCode>(normalized, true, out vk);
    }
}
