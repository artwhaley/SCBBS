# T06 — Stage E.0: Confirm HID interface is exclusively owned by kbdhid

**Stage:** E.0 (5-minute confirmation experiment)
**Estimated effort:** small
**Touches:** `TestEnumeratorAndClient/main.cpp`, `driverspike/phase2-references.md`

## Goal

Add a `--probe-hid` mode to the test client that:
1. Enumerates HID device interfaces.
2. Finds the device whose `HidD_GetAttributes` matches our VID/PID (or whose `HidP_GetCaps` reports Usage Page `0x01` / Usage `0x06`).
3. Attempts `CreateFile` with `GENERIC_READ | GENERIC_WRITE`, `FILE_SHARE_READ | FILE_SHARE_WRITE`.
4. If the open succeeds, attempts a no-op `HidD_GetFeature` or `HidD_SetOutputReport` (1-byte LED-shaped buffer with value 0).
5. Prints the exact result (HRESULT / `GetLastError()` value, decoded with `FormatMessage` if possible) for each step.
6. Records the result in `driverspike/phase2-references.md` Section 5.

This is a one-shot diagnostic; the output drives our confidence in the Stage E.1 design (custom interface). If `CreateFile` returns `ERROR_ACCESS_DENIED` (5), we've confirmed the assumption. If it succeeds, we pause and revisit the plan.

## Inputs

- Read first: `driverspike/phase2-plan.md` (Stage E.0).
- Read first: `driverspike/phase2-references.md` (Section 5 placeholder).
- Touch: `TestEnumeratorAndClient/main.cpp`, `driverspike/phase2-references.md`.

## Make ONLY these changes

1. In `TestEnumeratorAndClient/main.cpp`, add a `--probe-hid` argv mode. The existing `main()` should detect this argument and run a new function `ProbeHidExclusivity()` instead of any other behavior. When `--probe-hid` is not passed, the program should print a single line `Phase 2 transitional build — use --probe-hid (Stage E.0) or wait for T08 default mode.` and exit. (The phase 1 main flow is no longer reachable — that's expected.)
2. `ProbeHidExclusivity()` does the steps in the Goal section. For each step, print: step name, return value, `GetLastError()`, and a one-line `FormatMessage` decode of that error.
3. After the probe runs, append (do NOT overwrite) a new dated entry under Section 5 of `phase2-references.md` with: date, the probe's full text output, and a one-sentence interpretation (e.g., "ACCESS_DENIED on CreateFile confirms exclusive ownership; proceed with Path B custom interface as planned").
4. The probe does not need to be polished — fprintf to stderr/stdout is fine. No fancy formatting.

## Do NOT

- Change the driver, INF, common.h, or build script.
- Reuse phase 1's existing `FindMatchingDevice` if it depends on the old vendor usage page `0xFF00` — write a small parallel helper that filters on `UsagePage == 0x01 && Usage == 0x06`.
- Modify the phase 1 enumeration helpers in a way that would break their compilation — leave them dormant.
- Add a "fix" if `CreateFile` succeeds — just print and exit. We need the data point unmodified.
- Add new dependencies or libraries.

## Acceptance criteria (executor self-verification)

- [ ] `TestEnumeratorAndClient.exe --probe-hid` builds.
- [ ] Without `--probe-hid`, the binary prints the transitional message and exits.
- [ ] The probe handles "no matching device found" gracefully (prints a clear message, exits non-zero).
- [ ] Solution builds clean.
- [ ] Diff touches only `main.cpp` and `phase2-references.md`.

## Stop and ask if

- You can't determine the project's existing VID/PID values from `Device.c` or wherever they're set (do not invent values).
- `HidP_GetCaps` requires libraries not currently linked by `TestEnumeratorAndClient.vcxproj` (the project should already link `hid.lib`, `setupapi.lib`, `cfgmgr32.lib` from phase 1 — verify and report if not).

**NOTE TO USER:** This ticket only adds the probe code. The actual data collection (running `--probe-hid` after a fresh install and recording the output) is part of the manual test step in the user manual, not part of this ticket's executor scope.
