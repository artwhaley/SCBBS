using System.Runtime.InteropServices;
using StarCitizenButtonBoxServer.SpikeyHid;
using WindowsInput.Native;

namespace StarCitizenButtonBoxServer;

public sealed class SpikeyKeyboardBackend : IKeyboardInputBackend
{
    readonly SpikeyHidClient _client;
    readonly Action<string>? _log;

    public KeyboardBackendType BackendType => KeyboardBackendType.Spikey;

    public SpikeyKeyboardBackend(Action<string>? log = null)
    {
        _log = log;
        _client = new SpikeyHidClient(log);
    }

    public async Task ExecuteAsync(BindingEntry binding, CancellationToken cancellationToken = default)
    {
        var hasMouse = binding.MouseInputAction != MouseInputAction.None;
        var hasKey = !string.IsNullOrWhiteSpace(binding.Key);
        var hasModifiers = binding.LeftCtrl || binding.RightCtrl || binding.LeftShift ||
                           binding.RightShift || binding.LeftAlt || binding.RightAlt;
        if (!hasMouse && !hasKey && !hasModifiers) return;

        var hold = Math.Clamp(binding.HoldTimeMs, 1, 1050);

        try {
            if (binding.LeftCtrl) await _client.KeyDownAsync(0xE0, 0, cancellationToken);
            if (binding.RightCtrl) await _client.KeyDownAsync(0xE4, 0, cancellationToken);
            if (binding.LeftShift) await _client.KeyDownAsync(0xE1, 0, cancellationToken);
            if (binding.RightShift) await _client.KeyDownAsync(0xE5, 0, cancellationToken);
            if (binding.LeftAlt) await _client.KeyDownAsync(0xE2, 0, cancellationToken);
            if (binding.RightAlt) await _client.KeyDownAsync(0xE6, 0, cancellationToken);

            await Task.Delay(5, cancellationToken);

            if (hasMouse) {
                MouseDown(binding.MouseInputAction);
                await Task.Delay(hold, cancellationToken);
                MouseUp(binding.MouseInputAction);
            } else if (hasKey) {
                if (!TryResolveUsage(binding.Key!, out var usage)) {
                    _log?.Invoke($"[Spikey] Unmapped key '{binding.Key}', skipping command.");
                    return;
                }
                await _client.KeyDownAsync(usage, hold, cancellationToken);
                await Task.Delay(hold, cancellationToken);
                await _client.KeyUpAsync(usage, cancellationToken);
            } else {
                await Task.Delay(hold, cancellationToken);
            }
            await Task.Delay(5, cancellationToken);
        } finally {
            if (binding.RightAlt) await _client.KeyUpAsync(0xE6, cancellationToken);
            if (binding.LeftAlt) await _client.KeyUpAsync(0xE2, cancellationToken);
            if (binding.RightShift) await _client.KeyUpAsync(0xE5, cancellationToken);
            if (binding.LeftShift) await _client.KeyUpAsync(0xE1, cancellationToken);
            if (binding.RightCtrl) await _client.KeyUpAsync(0xE4, cancellationToken);
            if (binding.LeftCtrl) await _client.KeyUpAsync(0xE0, cancellationToken);
        }
    }

    public Task ReleaseAllAsync(CancellationToken cancellationToken = default) => _client.ReleaseAllAsync(cancellationToken);

    static bool TryResolveUsage(string keyText, out ushort usage)
    {
        usage = 0;
        if (!VirtualKeyHelper.TryResolveVirtualKey(keyText, out var vk)) return false;

        if (vk >= VirtualKeyCode.VK_A && vk <= VirtualKeyCode.VK_Z) {
            usage = (ushort)(0x04 + (vk - VirtualKeyCode.VK_A));
            return true;
        }
        if (vk >= VirtualKeyCode.VK_1 && vk <= VirtualKeyCode.VK_9) {
            usage = (ushort)(0x1E + (vk - VirtualKeyCode.VK_1));
            return true;
        }
        if (vk == VirtualKeyCode.VK_0) { usage = 0x27; return true; }
        if (vk >= VirtualKeyCode.F1 && vk <= VirtualKeyCode.F12) {
            usage = (ushort)(0x3A + (vk - VirtualKeyCode.F1));
            return true;
        }

        return vk switch
        {
            VirtualKeyCode.RETURN => Assign(0x28, out usage),
            VirtualKeyCode.ESCAPE => Assign(0x29, out usage),
            VirtualKeyCode.BACK => Assign(0x2A, out usage),
            VirtualKeyCode.TAB => Assign(0x2B, out usage),
            VirtualKeyCode.SPACE => Assign(0x2C, out usage),
            VirtualKeyCode.LEFT => Assign(0x50, out usage),
            VirtualKeyCode.RIGHT => Assign(0x4F, out usage),
            VirtualKeyCode.UP => Assign(0x52, out usage),
            VirtualKeyCode.DOWN => Assign(0x51, out usage),
            _ => false
        };
    }

    static bool Assign(ushort value, out ushort usage)
    {
        usage = value;
        return true;
    }

    public void Dispose() => _client.Dispose();

    const uint INPUT_MOUSE = 0;
    const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    const uint MOUSEEVENTF_LEFTUP = 0x0004;
    const uint MOUSEEVENTF_RIGHTDOWN = 0x0008;
    const uint MOUSEEVENTF_RIGHTUP = 0x0010;
    const uint MOUSEEVENTF_MIDDLEDOWN = 0x0020;
    const uint MOUSEEVENTF_MIDDLEUP = 0x0040;
    const uint MOUSEEVENTF_WHEEL = 0x0800;

    static void MouseDown(MouseInputAction action)
    {
        switch (action) {
            case MouseInputAction.LeftClick: SendMouseInput(MOUSEEVENTF_LEFTDOWN, 0); break;
            case MouseInputAction.RightClick: SendMouseInput(MOUSEEVENTF_RIGHTDOWN, 0); break;
            case MouseInputAction.MiddleClick: SendMouseInput(MOUSEEVENTF_MIDDLEDOWN, 0); break;
            case MouseInputAction.ScrollUp: SendMouseInput(MOUSEEVENTF_WHEEL, 120); break;
            case MouseInputAction.ScrollDown: SendMouseInput(MOUSEEVENTF_WHEEL, unchecked((int)0xFFFFFF88)); break;
        }
    }

    static void MouseUp(MouseInputAction action)
    {
        switch (action) {
            case MouseInputAction.LeftClick: SendMouseInput(MOUSEEVENTF_LEFTUP, 0); break;
            case MouseInputAction.RightClick: SendMouseInput(MOUSEEVENTF_RIGHTUP, 0); break;
            case MouseInputAction.MiddleClick: SendMouseInput(MOUSEEVENTF_MIDDLEUP, 0); break;
        }
    }

    static void SendMouseInput(uint flags, int data)
    {
        var input = new MOUSE_INPUT
        {
            type = INPUT_MOUSE,
            mi = new MOUSEINPUT { dwFlags = flags, mouseData = data }
        };
        SendInput(1, ref input, Marshal.SizeOf<MOUSE_INPUT>());
    }

    [DllImport("user32.dll")]
    static extern uint SendInput(uint nInputs, ref MOUSE_INPUT pInputs, int cbSize);

    [StructLayout(LayoutKind.Sequential)]
    struct MOUSE_INPUT { public uint type; public MOUSEINPUT mi; }

    [StructLayout(LayoutKind.Sequential)]
    struct MOUSEINPUT
    {
        public int dx, dy;
        public int mouseData;
        public uint dwFlags;
        public uint time;
        public IntPtr dwExtraInfo;
    }
}
