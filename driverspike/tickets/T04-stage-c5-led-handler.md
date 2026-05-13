# T04 — Stage C.5: LED output handler + idle posture

**Stage:** C.5 (lock IOCTL stub posture before Stage D)
**Estimated effort:** small
**Touches:** `TheSpikeyDriver/hid.c`

## Goal

Make sure the IOCTLs that `kbdhid` will start sending us once it attaches don't generate trace storms or device resets that could mask real bugs in Stage D. Specifically:

- Implement a minimal but precise LED output handler (validate length 1, ignore content, complete `STATUS_SUCCESS`, log via WPP).
- Confirm the idle notification IOCTL stays at `STATUS_NOT_IMPLEMENTED` (the documented-correct posture per `phase2-plan.md` Stage C.5).

## Inputs

- Read first: `driverspike/phase2-plan.md` (Stage C.5).
- Touch: `TheSpikeyDriver/hid.c` only.

## Make ONLY these changes

1. In `TheSpikeyDriver/hid.c`, modify the `IOCTL_HID_WRITE_REPORT` handling path (which `kbdhid` uses to send LED state). The current code calls `WriteReport(queueContext, Request)`. **Replace the body** of `WriteReport` (or add a sibling function `WriteReportKeyboardLeds` and call it) so that it:
   - Calls `RequestGetHidXferPacket_ToWriteToDevice(Request, &packet)`.
   - On failure, returns the status (no change).
   - Validates `packet.reportId == 0` (no Report IDs in our descriptor) — return `STATUS_INVALID_PARAMETER` if not.
   - Validates `packet.reportBufferLen == 1` (1-byte LED report) — return `STATUS_INVALID_BUFFER_SIZE` if not.
   - Logs at TRACE_LEVEL_INFORMATION via WPP: `"LED report received, byte=0x%02X"`.
   - Sets `WdfRequestSetInformation(Request, 1)` and returns `STATUS_SUCCESS`. Does NOT touch any device-context state (LEDs are just acknowledged, not stored).
2. Confirm (read-only) that the `IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST` case in the `EvtIoDeviceControl` switch still returns `STATUS_NOT_IMPLEMENTED` (it should be in the `default` cluster). No code change required — just confirm in your report.

## Do NOT

- Change `IOCTL_HID_READ_REPORT` (Stage D ticket).
- Change descriptor, common.h, INF, manual queue, or test client.
- Add a real LED-state device context field (we don't track LED state).
- Add a separate handler for `IOCTL_UMDF_HID_SET_OUTPUT_REPORT` — `kbdhid` uses `IOCTL_HID_WRITE_REPORT`, not the UMDF feature variant.
- Refactor anything else.

## Acceptance criteria (executor self-verification)

- [ ] `WriteReport` (or its replacement) validates length == 1 and reportId == 0, logs via WPP, returns SUCCESS.
- [ ] Solution builds clean: `& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd"` exits 0.
- [ ] Diff touches only `TheSpikeyDriver/hid.c`.

## Stop and ask if

- You're unsure whether `kbdhid` uses `IOCTL_HID_WRITE_REPORT` or `IOCTL_UMDF_HID_SET_OUTPUT_REPORT` for LEDs (do not guess — flag it and we'll decide).
- The WPP trace macro for "INFORMATION" level isn't obvious from existing trace calls in `hid.c`.
