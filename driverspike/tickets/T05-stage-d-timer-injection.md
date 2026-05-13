# T05 — Stage D: Driver-side hardcoded timer injection

**Stage:** D (first true injection proof)
**Estimated effort:** small-medium
**Touches:** `TheSpikeyDriver/manualqueue.c`, `TheSpikeyDriver/Device.h` (small context addition)

## Goal

Modify the driver-side periodic timer to alternate "press 'A'" / "release all" reports every 2 seconds. With the test app NOT running, focusing Notepad should show 'A' characters appearing on a 2-second cadence. This proves the keyboard stack is consuming our reports end to end.

## Inputs

- Read first: `driverspike/phase2-plan.md` (Stage D, Critical Invariants).
- Read first: `driverspike/phase2-references.md` (HID usage table — `'A'` is `0x04`).
- Touch: `TheSpikeyDriver/manualqueue.c`, `TheSpikeyDriver/Device.h`.

## Make ONLY these changes

1. In `TheSpikeyDriver/manualqueue.c`:
   - Change `TIMER_PERIOD_MS` from `1000u` to `2000u` (2-second cadence; satisfies invariant #6 of ≥500 ms with margin).
   - Add a small static or device-context BOOLEAN `TogglePressedState` (see step 2). Default false.
   - Replace the body of `TheSpikeyDriverEvtTimerFunc` with this logic:
     1. Retrieve the next pending request from the manual queue.
     2. If `WdfIoQueueRetrieveNextRequest` fails (no read pending), log a `TRACE_LEVEL_WARNING` WPP message `"Timer fired but no read pending"` and return. **Do not** retry, do not sleep, do not store the report for next time.
     3. If a request was retrieved, build a `HIDMINI_INPUT_REPORT` initialized to all zeros. Then, if `TogglePressedState` is currently false, set `report.Keycodes[0] = 0x04` ('A' HID usage). If true, leave all zeros (release). Toggle the boolean.
     4. Copy the 8-byte report into the request via `RequestCopyFromBuffer` and complete with the returned status.
2. In `TheSpikeyDriver/Device.h`, add a `BOOLEAN TogglePressedState;` field to `DEVICE_CONTEXT` (initialized to FALSE by zero-init via `WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE`). Use this in step 1 instead of a static (UMDF can have multiple instances; per-device state is correct).
3. **Baseline-zero invariant:** in `TheSpikeyDriverManualQueueInitialize` (`manualqueue.c`), after the timer is created but before `WdfTimerStart`, the device's `TogglePressedState` should already be FALSE. The first timer fire will produce an "all-zeros release" report, which serves as the baseline. Add a one-line comment at that location: `// First timer fire emits all-zeros baseline (invariant #5).`

## Do NOT

- Change the descriptor, common.h, INF, or LED handler.
- Change the test client.
- Add an immediate-injection IOCTL path — Stage E does that.
- Disable or remove the periodic timer — Stage E may or may not depend on it.
- Try to "make injection more responsive" by speeding the timer up below 2000 ms.
- Refactor `manualqueue.c` for clarity beyond what these steps require.

## Acceptance criteria (executor self-verification)

- [ ] `TIMER_PERIOD_MS == 2000u`.
- [ ] `EvtTimerFunc` logs a WPP warning when no read is pending.
- [ ] `TogglePressedState` lives in `DEVICE_CONTEXT`, not a static or global.
- [ ] First report after device start is all-zeros (baseline).
- [ ] Solution builds clean.
- [ ] Diff touches only the two files listed.

## Stop and ask if

- The HID usage code for 'A' in `phase2-references.md` is anything other than `0x04`.
- `RequestCopyFromBuffer` doesn't accept the 8-byte report cleanly (signature mismatch suggests something else is wrong).
- You see anywhere else in `manualqueue.c` that depends on the old `readReport.ReportId` or `.Data` (T03 should have cleaned that up — flag if it didn't).
