# T03 — Stage C: Descriptor swap to Generic Desktop / Keyboard

**Stage:** C (the moment of truth — kbdhid binding test)
**Estimated effort:** medium
**Touches:** `driverspike-common.h`, `TheSpikeyDriver/hid.c`, `TheSpikeyDriver/manualqueue.c`, `TestEnumeratorAndClient/main.cpp`

## Goal

Swap the report descriptor from vendor-defined to Generic Desktop / Keyboard (Usage Page `0x01`, Usage `0x06`). Drop Report IDs entirely (no `0x85` opcode). Restructure `HIDMINI_INPUT_REPORT` to be 8 bytes total (no ReportId field), matching the boot keyboard layout. After this ticket, `kbdhid.sys` should attach.

## Inputs

- Read first: `driverspike/phase2-plan.md` (Stage C section, Critical Invariants).
- Read first: `driverspike/phase2-references.md` (descriptor bytes are the source of truth).
- Touch: `driverspike-common.h`, `TheSpikeyDriver/hid.c`, `TheSpikeyDriver/manualqueue.c`, `TestEnumeratorAndClient/main.cpp`.

## Make ONLY these changes

1. In `driverspike-common.h`:
   - Replace `HIDMINI_INPUT_REPORT` with a struct (still using `pshpack1.h`) of exactly 8 bytes: `unsigned char Modifier; unsigned char Reserved; unsigned char Keycodes[6];`. **No `ReportId` field.**
   - The Stage B macros (`INPUT_REPORT_SIZE_CB` etc.) need to be reviewed: `INPUT_REPORT_SIZE_CB` should now equal `sizeof(HIDMINI_INPUT_REPORT)` (== 8), NOT `sizeof - 1`. Fix this.
   - Leave `HIDMINI_OUTPUT_REPORT` and `HIDMINI_CONTROL_INFO` untouched — they are now dead code consumed only by orphaned vendor IOCTL handlers, which we'll handle below. Do NOT delete them; they keep the test app compilable.
2. In `TheSpikeyDriver/hid.c`:
   - Replace the bytes inside `G_DefaultReportDescriptor[]` with the boot keyboard descriptor bytes from `driverspike/phase2-references.md` Section 1, exactly. Preserve the per-line `//` comments from the reference doc.
   - In the `EvtIoDeviceControl` switch, the `IOCTL_HID_WRITE_REPORT` / `IOCTL_UMDF_HID_GET_FEATURE` / `IOCTL_UMDF_HID_SET_FEATURE` / `IOCTL_UMDF_HID_GET_INPUT_REPORT` paths previously checked `packet.reportId == CONTROL_COLLECTION_REPORT_ID`. Since we now have no Report IDs, change those checks to: if `packet.reportId != 0` return `STATUS_INVALID_PARAMETER`. (The HID stack uses `reportId == 0` to mean "the unique report" when no IDs are declared.) Do not delete these handlers — leave them as is otherwise; they will go unused but must compile.
   - The `IOCTL_HID_READ_REPORT` path forwards to the manual queue — no changes needed here.
   - Leave `GetString`, `GetIndexedString`, descriptor IOCTLs, idle/activate/deactivate stubs as-is.
3. In `TheSpikeyDriver/manualqueue.c`:
   - The timer's `readReport` is now a `HIDMINI_INPUT_REPORT` of 8 bytes with no ReportId field. Initialize all 8 bytes to zero (`RtlZeroMemory(&readReport, sizeof(readReport));`). Remove the line that assigned `readReport.ReportId = CONTROL_FEATURE_REPORT_ID;`.
   - Remove the line `readReport.Data = queueContext->DeviceContext->DeviceData;` since the new struct has no `Data` field of that shape. The timer in this stage just emits all-zeros (Stage D will replace this logic with key alternation).
4. In `TheSpikeyDriver/hid.c`, the `GetInputReport` function (around line 290) currently writes `reportBuffer->ReportId = CONTROL_COLLECTION_REPORT_ID;` and `reportBuffer->Data = QueueContext->OutputReport;`. Both fields no longer exist after the struct change. Replace those two lines with `RtlZeroMemory(reportBuffer, sizeof(HIDMINI_INPUT_REPORT));`. The function still returns a valid (all-zeros) input report; this IOCTL path is now effectively dead code for the keyboard, but must compile. Do not delete the function or the case in the IOCTL switch.
5. In `TestEnumeratorAndClient/main.cpp`, the existing helpers `GetInputReport` (around line 232) and `ReadInputData` (around line 273) reference `HIDMINI_INPUT_REPORT.Data` and `.ReportId`. These functions are about to become dead code (T06 replaces `main()`'s entry point). To make the project compile in this ticket without rewriting the test app:
   - Locate the line `printf("GetInputReport Data=%u\n", ((PHIDMINI_INPUT_REPORT)buffer)->Data);` and change it to `printf("GetInputReport returned (phase 2 — keyboard struct, dead code)\n");`. Delete just the broken `->Data` reference inline; do not modify any surrounding logic.
   - Locate the line `report->ReportId = CONTROL_COLLECTION_REPORT_ID;` in `ReadInputData` and replace the entire body of that loop iteration with a single `printf("ReadInputData (phase 2 — keyboard struct, dead code)\n"); break;` so the loop exits immediately. The function still returns success/failure as before. Do not refactor the function signature.
   - Make no other changes to the test app. Phase 1 helpers that don't touch `HIDMINI_INPUT_REPORT` (e.g., `GetFeature`, `SetFeature`, `SetOutputReport` which use `HIDMINI_OUTPUT_REPORT` / `HIDMINI_CONTROL_INFO`) keep compiling because we're leaving those structs alone.

## Do NOT

- Change the INF, VID/PID, the test client, the build script, or any phase 1 document.
- Delete `HIDMINI_OUTPUT_REPORT`, `HIDMINI_CONTROL_INFO`, or any vendor IOCTL handler — they become unused but must continue to compile.
- Touch the LED / output report handler logic (T04 handles output reports).
- Modify the test app (`TestEnumeratorAndClient/main.cpp`) beyond the two minimal stub edits in step 5 above. The full main-flow rewrite happens in T06 and T08. We don't smoke-test the test app's behavior during T03–T05; we only need it to compile.
- "Improve" anything you notice in passing.

## Acceptance criteria (executor self-verification)

- [ ] No `0x85` byte appears in `G_DefaultReportDescriptor[]`.
- [ ] `sizeof(HIDMINI_INPUT_REPORT) == 8`.
- [ ] `INPUT_REPORT_SIZE_CB == 8`.
- [ ] Solution builds clean: `& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd"` exits 0. (Both the driver project AND the test client project must compile — the test client may emit linker warnings about unused functions, that's fine.)
- [ ] Diff touches only the four files listed.

## Stop and ask if

- The reference descriptor bytes don't sum to 64 input-report bits (you computed wrong, or the reference is wrong — say which).
- A handler in `hid.c` references `HIDMINI_INPUT_REPORT.ReportId` or `.Data` directly and you can't see how to make it compile after the struct change.
- Anything in `Device.c` or `Queue.c` uses `INPUT_REPORT_SIZE_CB` or the input report struct (the assumption is they don't — verify and report if they do).
