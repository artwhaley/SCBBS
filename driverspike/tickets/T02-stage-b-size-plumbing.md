# T02 — Stage B: Size plumbing only (vendor usage retained)

**Stage:** B (plumbing-only; kbdhid is NOT exercised)
**Estimated effort:** small
**Touches:** `driverspike-common.h`, `TheSpikeyDriver/hid.c`, `TheSpikeyDriver/manualqueue.c`

## Goal

Grow the input report payload to 8 bytes while staying on vendor usage page `0xFF00`. Prove the size-change machinery (descriptor length, struct size, manual queue copy size, `HidP_GetCaps` reported size) all stays consistent. **Report ID is retained in this stage** — total report length becomes 9 (1 byte report ID + 8 bytes payload).

This stage does not touch the keyboard stack at all. The device still enumerates as a vendor HID device. The phase 1 test app should continue to work essentially as before, just reporting a larger `InputReportByteLength`.

## Inputs

- Read first: `driverspike/phase2-plan.md` (Stage B section).
- Read first: `driverspike/phase2-references.md` (T01 output).
- Touch: `driverspike/driverspike-common.h`, `driverspike/TheSpikeyDriver/hid.c`, `driverspike/TheSpikeyDriver/manualqueue.c`.

## Make ONLY these changes

1. In `driverspike-common.h`, change `HIDMINI_INPUT_REPORT` so its `Data` field becomes `unsigned char Data[8]` (8 bytes of payload). Do not change `ReportId`. Do not touch any other struct (control info, output report).
2. The descriptor literal in `TheSpikeyDriver/hid.c` (`G_DefaultReportDescriptor`) currently uses `INPUT_REPORT_SIZE_CB` macro — verify this macro automatically picks up the new size via `sizeof(HIDMINI_INPUT_REPORT) - 1`. **No descriptor opcode changes are allowed in this ticket.**
3. In `TheSpikeyDriver/manualqueue.c`, the `EvtTimerFunc` writes a `HIDMINI_INPUT_REPORT` to the request — confirm the existing code still compiles and the size used is `sizeof(readReport)`. If a fixed-size literal is hard-coded, replace it with `sizeof(readReport)`. (Most likely no change needed — verify by reading.)
4. Confirm `TheSpikeyDriver/hid.c` `GetInputReport` and `ReadReport` paths still use `sizeof(HIDMINI_INPUT_REPORT)` for size math (not a hardcoded `2`). If they hardcode, fix to `sizeof`. Otherwise no change.

## Do NOT

- Change the descriptor opcodes or usage page (Stage C does that).
- Drop the `ReportId` field (Stage C does that).
- Modify the INF, the test client, the build script, or any phase 1 document.
- Refactor any function for clarity, naming, or "while we're here" cleanup.
- Touch `Driver.c`, `Device.c`, `Queue.c`, `util.c`, or any `.h` other than `common.h`.
- Modify the LED / output report handler.

## Acceptance criteria (executor self-verification before reporting done)

- [ ] `sizeof(HIDMINI_INPUT_REPORT)` is now `1 + 8 = 9` bytes.
- [ ] Solution builds clean: run from a non-elevated PowerShell:
  `& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd"`
  and verify exit code 0 and no compiler warnings related to your changes.
- [ ] Diff is small and only touches the four files listed above (plus possibly auto-generated build output, which is fine).

## Stop and ask if

- Any of the listed files don't exist where expected.
- Build fails with errors that look unrelated to your changes.
- You discover a hardcoded size literal in a fifth file — list it and ask before modifying.
