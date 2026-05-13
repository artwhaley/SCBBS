# T07 — Stage E.1 (driver side): Custom device interface + injection IOCTL

**Stage:** E.1 driver work
**Estimated effort:** medium
**Touches:** `TheSpikeyDriver/Public.h`, `TheSpikeyDriver/Device.h`, `TheSpikeyDriver/Device.c`, `TheSpikeyDriver/hid.c`, `driverspike-common.h`

## Goal

Publish a custom device interface from the UMDF driver so the test app can talk to it via standard `DeviceIoControl` (bypassing the kbdhid-claimed HID path). Add one private IOCTL `IOCTL_THESPIKEYDRIVER_INJECT_KEY` that takes a single byte (HID usage code) and triggers a key-down report followed shortly by a key-up report on the keyboard interface.

The keyboard channel from T05 (timer alternating 'A') must continue to work in parallel — confirm by leaving the timer running but downgrading its action: instead of alternating, the timer should now just emit all-zeros (release) every tick. The IOCTL is what produces actual keystrokes.

## Inputs

- Read first: `driverspike/phase2-plan.md` (Stage E section, Stage E.1).
- Read first: `driverspike/phase2-references.md`.
- Touch: `Public.h`, `Device.h`, `Device.c`, `hid.c`, `driverspike-common.h`, `manualqueue.c`.

## Make ONLY these changes

1. **`TheSpikeyDriver/Public.h`**: Define a new `GUID` `GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL` using a freshly generated UUID (use `uuidgen` or a Win32 GUID generator — do NOT reuse any historic GUID from phase 1 git history). Define it via `DEFINE_GUID(...)` macro. This file currently has no real content — replace its body but keep the include guard.
2. **`driverspike-common.h`**:
   - Add include `<initguid.h>`-friendly forward — NO, do not add includes here. Instead, mirror `Public.h` with an `extern "C"` declaration of the same GUID for the user-mode side. Easier: in `common.h` define the IOCTL code and the payload struct only. The GUID definition lives in `Public.h` (driver side) and a parallel `extern const GUID` declaration usable by the test app.
   - Add: `#define IOCTL_THESPIKEYDRIVER_INJECT_KEY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)`.
   - Add struct: `typedef struct _THESPIKEYDRIVER_INJECT_KEY_INPUT { unsigned char HidUsage; unsigned char Modifier; } THESPIKEYDRIVER_INJECT_KEY_INPUT, *PTHESPIKEYDRIVER_INJECT_KEY_INPUT;` (2 bytes; modifier defaults to 0 for the spike's lowercase 'i' but the field exists for future use).
3. **`TheSpikeyDriver/Device.c`**, in the device add / `TheSpikeyDriverCreateDevice` flow, after `WdfDeviceCreate` succeeds: call `WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL, NULL)` and check the return value. WPP-trace failure with the status code. Do NOT add a reference string.
4. **`TheSpikeyDriver/Device.h`**: Add device-context fields `UCHAR PendingInjectUsage; BOOLEAN InjectPending;` (purpose: the IOCTL stashes the requested usage code; the next timer tick or a single-shot timer flushes it as a key-down, then schedules a key-up).
5. **`TheSpikeyDriver/hid.c`**, in the `EvtIoDeviceControl` switch, **after** the existing HID-IOCTL `default` cluster but **before** the `default` of the outer switch, add a case for `IOCTL_THESPIKEYDRIVER_INJECT_KEY`:
   - `WdfRequestRetrieveInputBuffer` for `sizeof(THESPIKEYDRIVER_INJECT_KEY_INPUT)`.
   - On failure, complete with the error.
   - On success, copy the byte into `deviceContext->PendingInjectUsage`, set `deviceContext->InjectPending = TRUE`, complete the IOCTL with `STATUS_SUCCESS`.
   - WPP trace at INFORMATION level: `"Inject IOCTL received, usage=0x%02X, modifier=0x%02X"`.
6. **`TheSpikeyDriver/manualqueue.c`**, `EvtTimerFunc`: change the body so the report it sends depends on `deviceContext->InjectPending`:
   - If `InjectPending` is TRUE: build a key-down report (`Modifier = saved-modifier; Keycodes[0] = PendingInjectUsage`), complete the request, then **set `InjectPending` to FALSE** so the next tick emits all-zeros (the key-up). The "press 'A' alternating" logic from T05 is removed — the timer no longer drives any test pattern. Its job is now: emit injection-pending reports, then emit baseline zeros.
   - If `InjectPending` is FALSE: emit all-zeros report (key-up / idle). This guarantees a key-up follows every key-down within one timer period (max 2 seconds). For the spike, that latency is fine.
   - Keep the "no read pending" WPP warning from T05.

## Do NOT

- Touch the INF (the custom device interface is published by the driver at runtime, no INF entry needed for the GUID).
- Touch the test client (T08 does that).
- Reuse a phase-1 GUID from git history — generate a fresh one and write it inline.
- Add registry entries for the custom interface — `WdfDeviceCreateDeviceInterface` is sufficient.
- Add KMDF-style `WdfDeviceCreateSymbolicLink` calls — UMDF uses the device interface mechanism.
- Implement an immediate single-shot WDF timer for key-up — relying on the next periodic tick is simpler and good enough for this spike.

## Acceptance criteria (executor self-verification)

- [ ] `Public.h` defines `GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL` with a fresh GUID.
- [ ] `IOCTL_THESPIKEYDRIVER_INJECT_KEY` is defined in `common.h` using `CTL_CODE` in the user-defined range (`0x800–0xFFF`).
- [ ] `Device.c` calls `WdfDeviceCreateDeviceInterface` after device create.
- [ ] `EvtIoDeviceControl` handles the new IOCTL.
- [ ] Timer no longer alternates 'A' — it emits keys based on `InjectPending`.
- [ ] Solution builds clean.
- [ ] Diff touches only the listed files.

## Stop and ask if

- `WdfDeviceCreateDeviceInterface` requires INF entries for the GUID under UMDF (it shouldn't, but confirm if you find docs saying otherwise).
- The IOCTL switch in `hid.c` already has a different handler for `0x800`-range codes (it shouldn't — check phase 1 history).
- You're unsure whether `EvtIoDeviceControl` is the right entry point for non-HID IOCTLs (it is — UMDF routes all IOCTLs through `EvtIoDeviceControl` regardless of whether they're HID or vendor).
