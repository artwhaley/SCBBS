# Spikey Keystroke Injector GUI

Small Win32 test utility for the `TheSpikeyDriver` custom injection IOCTL.

## Build

From the `driverspike` folder:

```powershell
msbuild .\driverspike.sln /t:KeystrokeInjectorGui /p:Configuration=Debug /p:Platform=x64
```

The solution build writes:

```text
x64\Debug\KeystrokeInjectorGui.exe
```

## Use

1. Start `KeystrokeInjectorGui.exe`.
2. Enter a keystroke.
3. Click `Start 10s Countdown`.
4. Focus the target app before the countdown reaches zero.

Accepted examples:

- `a`
- `A`
- `Ctrl+A`
- `Enter`
- `Space`
- `F1`
- `NumLock`
- `0x04`

The GUI uses the same `GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL` and `IOCTL_THESPIKEYDRIVER_INJECT_REPORT` protocol as `TestEnumeratorAndClient.exe`.

## Diagnostics

The diagnostics panel shows:

- control-interface list length and count
- the first discovered interface path
- `CreateFile` result
- injected usage, modifier, and emit count
- `DeviceIoControl` result and `GetLastError`
- `GET_STATS` output when available

Use `Refresh Stats` before and after injection while debugging the driver ticket stack.

## Notes

The driver must be installed and the control interface must be openable. If the ticket stack is not finished yet, the GUI can build and run but injection may report an open or IOCTL failure.
