# Phase 2 Ticket Manual

How to drive the Phase 2 spike one ticket at a time, with a tightly-bounded executor agent doing the editing and you doing the install/test.

## One-time setup (do these before T01)

1. **Tag the phase 1 baseline.** From a non-elevated PowerShell at `C:\Users\artwh\Downloads\Star Commander MFD`:
   ```powershell
   git tag phase1-baseline
   ```
   This is the rollback point if any ticket goes badly wrong.

2. **Open two terminal windows and keep them open for the whole spike.**
   - **Window 1: Build/test (non-elevated PowerShell).** Working dir: `C:\Users\artwh\Downloads\Star Commander MFD\driverspike`. You'll run `Build-VS2022.cmd` here for compile checks.
   - **Window 2: Install (elevated PowerShell — "Run as Administrator").** Working dir same. You'll run `Redeploy-TheSpikeyDriver.ps1` and `devcon` commands here.

3. **Keep `phase2-plan.md`, `phase2-references.md`, and the current ticket file open in your editor** — you'll cross-reference them while reviewing the executor's diff before installing.

4. **WPP trace viewer.** If you don't already have a TraceView session set up against `TheSpikeyDriver`'s WPP GUID from phase 1, set one up now. (We won't re-document it — same GUID and process as phase 1.)

5. **`Redeploy-TheSpikeyDriver.ps1` does not need to change for any Phase 2 ticket.** It locates the driver by file name (`TheSpikeyDriver.inf` / `.cat` / `.dll`) and hardware ID (`root\TheSpikeyDriver`) — none of which Phase 2 modifies. If a future ticket ever changes those, that's a flag to update the script in the same ticket. Phase 2 as planned has no such ticket.

## Per-ticket workflow

For every ticket you do the same six steps:

1. **Open a fresh executor window** (no prior ticket context). Paste the prompt block from this manual.
2. **Wait for it to report done.** Read its summary. It must say "diff touches only [files]" — if it says it touched anything not in the ticket's allowlist, **stop and ask me before continuing**.
3. **Sanity-review the diff yourself.** `git diff` from Window 1. You're looking for: scope creep, deletions of things the ticket didn't ask to delete, refactoring not requested. If you see any of those, undo them or ask me.
4. **Run the build verify** from Window 1: `& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd"`. Must exit with errorlevel 0.
5. **Run the test/install steps** from this manual (per-ticket section below). Most tickets need elevated install via Window 2.
6. **Commit the ticket's work** before moving on, so next ticket's diff is clean: `git add -A && git commit -m "T0X: <title>"`.

## Context strategy

**Use a fresh executor window per ticket.** Do not let the executor accumulate context from previous tickets — that's how it starts "remembering" decisions and inventing scope. The ticket file + the plan file + the reference file is all the context it should ever need. If you want to spot-check by re-prompting in the same window, that's fine, but start each ticket fresh.

## When to stop and come back to me (decision points)

These are the moments you should NOT just press on:

- **After T01 (Stage A complete):** Read the descriptor doc yourself before any code changes. If anything in `phase2-references.md` looks wrong against your own reading of the spec, stop. (Decision point: data quality of references.)
- **After T03 (Stage C — kbdhid binding):** This is the single biggest go/no-go in Phase 2. Run the verification sequence. If `devcon stack` does NOT show `kbdhid` above `mshidumdf`, stop. Don't proceed to T04. We need to debug binding before doing anything else. (Decision point: kbdhid actually attached?)
- **After T05 (Stage D — first injection):** If 'A' does not appear in Notepad on a 2-second cadence, stop. Don't proceed to T06. The plan's "Known failure signatures" table is your starting checklist; come back to me with what you found. (Decision point: end-to-end report path proven?)
- **After T06 (Stage E.0 — probe result):** If the probe reports `CreateFile` SUCCEEDED, stop and bring me the result. The Stage E plan was designed assuming the probe fails with ACCESS_DENIED. (Decision point: assumption validated?)
- **After T08 (Stage E.1 — full UX):** If 'i' does not appear in Notepad after press-Enter-countdown, stop. The plan's failure-signatures table covers the likely causes. (Decision point: spike acceptance reached?)
- **Any time the executor says "stop and ask"** in its summary, do not press on — bring its question to me.
- **Any time you've installed and the driver fails to start (PnP problem code != 0):** stop. Two install attempts max per the project's stop rule (`orchestrator-prompt.md`).

---

## Universal prompt scaffold

Every ticket prompt has this shape. The executor needs to know its scope is hard-bounded.

```
You are working on the driverspike Phase 2 spike. Your scope for this session is exactly one ticket. Do not exceed it.

Read these files in this order:
1. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\phase2-plan.md
2. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\phase2-references.md
3. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\<TICKET FILE NAME>

Then implement the "Make ONLY these changes" section of the ticket.

Hard rules:
- Do not modify any file not explicitly listed in the ticket's "Make ONLY these changes" section.
- Do not refactor, rename, reorganize, or "clean up" anything that the ticket does not require.
- If you notice a bug, code smell, or inconsistency unrelated to this ticket, leave it alone and mention it in your final report. Do not fix it.
- If anything in the ticket is ambiguous, STOP and ask, do not guess.
- If the ticket says "Stop and ask if X" and X happens, STOP and ask.
- Do not run the driver build, do not run install commands, do not run tests. The user does that. Your job ends when the source code matches the ticket.
- Do not commit to git. The user does that.

When you are done, report back with:
- "Diff touches only: [list of files]"
- A 3-line summary of what you changed.
- Anything you stopped and didn't do, with the reason.
- Any "Stop and ask" condition you hit (or "none triggered").
```

Use this scaffold verbatim for every ticket. Below, each ticket has a "Hand to executor" line — that's just the ticket file name to substitute in.

---

## T01 — Stage A: References

**Hand to executor:** Substitute `tickets\T01-stage-a-references.md` into the universal prompt.

**You verify after:**
- Open `driverspike/phase2-references.md`. Read sections 1–5 yourself.
- Compute the descriptor's input report size by hand: sum `Report Size × Report Count` for each Input item. Must equal 64 bits.
- Confirm `'i' = 0x0C` in section 2.

**No install / no driver work this ticket.**

**Decision point:** **Does the descriptor look right?** If anything looks off, do not start T02. Bring it to me.

**Then:** `git add -A && git commit -m "T01: phase2 references doc"`.

---

## T02 — Stage B: Size plumbing

**Hand to executor:** `tickets\T02-stage-b-size-plumbing.md`.

**You verify after:**

Build (Window 1):
```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd"
```

Install + smoke (Window 2, elevated):
```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1"
```

Verify install succeeded (Window 2):
```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'ROOT\TheSpikeyDriver*' } | Select-Object Name, Service, Status, ConfigManagerErrorCode
```
Expect `ConfigManagerErrorCode = 0` and `Status = OK`.

Run the existing test app (no args — phase 1 behavior expected to still work since usage is still vendor `0xFF00`):
```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\x64\Debug\TestEnumeratorAndClient\TestEnumeratorAndClient.exe"
```
Look for it to find the device and report `InputReportByteLength` of 9 (1 byte report ID + 8 bytes payload). If it reports anything else, stop.

**Decision point:** none — should pass cleanly. If it doesn't, the size-change machinery is broken and you should bring me the test app output before T03.

**Then:** commit T02.

---

## T03 — Stage C: Keyboard descriptor swap (the moment of truth)

**Hand to executor:** `tickets\T03-stage-c-keyboard-descriptor.md`.

**You verify after:**

Build (Window 1):
```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd"
```

Install (Window 2, elevated):
```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1"
```

**Acceptance hierarchy — check in this order, each gates the next:**

1. **HIDView parses descriptor cleanly.** Path: `C:\Program Files (x86)\Windows Kits\10\Tools\<ver>\x64\hidclient.exe`. Find your device in the list, view the descriptor — should show Usage Page 0x01, Usage 0x06, no errors. **If HIDView reports descriptor errors, stop here — bytes are wrong.**
2. **`HidP_GetCaps` reports the right shape.** No quick CLI for this — quickest check: HIDView's "Device Capabilities" view shows InputReportByteLength. Expect **8**. If 9, you still have a Report ID byte somewhere.
3. **kbdhid service binding (THE authoritative go/no-go).** Window 2:
   ```powershell
   Get-CimInstance Win32_PnPEntity |
     Where-Object { $_.Service -eq 'kbdhid' -and $_.PNPDeviceID -like '*VID_*' } |
     Select-Object Name, PNPDeviceID, Service, Status
   ```
   At least one row, `Status = OK`. The PNPDeviceID will start with `HID\VID_…` — that's the *child* device hidclass spawned for our keyboard collection. If empty: descriptor isn't being consumed as a keyboard. **This is the primary signal — not devcon stack.**
4. **Parent UMDF stack sanity (NOT a kbdhid check).** Window 2:
   ```powershell
   $devcon = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Tools" -Filter devcon.exe -Recurse | Select-Object -First 1
   & $devcon.FullName stack 'root\TheSpikeyDriver'
   ```
   Confirms `mshidumdf` and `WUDFRd` are present — proves the UMDF side is healthy. **`kbdhid` will NOT appear here**, that's expected (it lives on the child PDO). See `phase2-references.md` §3 for the full picture.
5. **Optional — inspect the child PDO directly.** Get the child's instance ID from step 3's output, then:
   ```powershell
   & $devcon.FullName stack "HID\VID_*"
   ```
   You should see `kbdhid` as Controlling Service and `kbdclass` as Upper Filter. Useful for triage; redundant with step 3.
6. **Reboot, then re-run steps 3 and 4.** They must still pass.
7. **`devcon restart`:**
   ```powershell
   & $devcon.FullName restart 'root\TheSpikeyDriver'
   ```
   Then re-run step 3.

**Decision point — STRONG:** This is the biggest go/no-go in Phase 2. **If steps 1, 2, 3 don't all pass, do NOT start T04.** Bring me the failing step's output. The plan's "Known failure signatures" table maps symptoms to causes. (Step 4 failing alone means the UMDF side is broken, which is a different debug — also stop and bring it to me, but don't conflate it with kbdhid.)

**Then:** commit T03.

---

## T04 — Stage C.5: LED handler

**Hand to executor:** `tickets\T04-stage-c5-led-handler.md`.

**You verify after:**

Build + install as in T02/T03.

Verify driver still installs and starts:
```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'ROOT\TheSpikeyDriver*' } | Select-Object Name, Status, ConfigManagerErrorCode
```

**Toggle a LED** to exercise the new handler. The cleanest way: focus a Notepad window or any text input, then tap CapsLock or NumLock on your real keyboard. The OS broadcasts the new LED state to all attached keyboards including ours. Watch your WPP trace session for the `"LED report received, byte=0x..."` INFO line.

If you don't see the LED trace within a few seconds of toggling CapsLock, two possibilities:
- WPP session isn't capturing INFO level (raise it).
- `kbdhid` isn't broadcasting LEDs to virtual keyboards (possible — non-fatal; just means the handler is dormant). Note this and continue.

**Idle Notification check:** Just verify (read the code yourself or have the executor confirm in its report) that `IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST` still returns `STATUS_NOT_IMPLEMENTED`. Look at WPP for any storm of repeated errors during the next 60 seconds of idle. If the trace is quiet, you're good.

**Decision point:** Only if you see an error storm — bring it to me. Otherwise proceed.

**Then:** commit T04.

---

## T05 — Stage D: Hardcoded timer injection (first real keystrokes)

**Hand to executor:** `tickets\T05-stage-d-timer-injection.md`.

**You verify after:**

Build + install as before.

**The test:**
1. Open Notepad (`notepad.exe`). Make sure it has focus.
2. Wait. Within 4 seconds you should see `A` appear, then nothing for 2 seconds, then `A` again, etc. (Each press-A → release-A pair takes one full timer cycle, so character cadence is roughly one per 4 seconds.)
3. Click somewhere else, return to Notepad — typing should resume.

**WPP traces to watch:** `"Timer fired but no read pending"` warnings should be rare or absent during steady state. If you see them constantly, the manual queue isn't being kept fed by `kbdhid` — flag it.

**No stuck keys check:** Close Notepad. Open something else (a file dialog, a browser address bar). Look for phantom 'A' characters appearing where you didn't ask. If you see any, the baseline-zero invariant was violated. Stop and bring me the trace.

**Reboot test:** After confirming it works, reboot. Open Notepad. Same behavior should appear.

**Decision point — STRONG:** If 'A' does NOT appear in Notepad on a ~4-second cadence, do NOT start T06. The most likely issues (in order):
- Wrong HID usage code (sent ASCII 0x41 instead of HID 0x04).
- Report ID byte snuck in.
- Timer never gets pending reads (manual queue not properly registered for `IOCTL_HID_READ_REPORT`).
- kbdhid never attached (rerun T03 verification).

Bring me the WPP trace + `devcon stack` output.

**Then:** commit T05.

---

## T06 — Stage E.0: Probe whether HID interface is openable

**Hand to executor:** `tickets\T06-stage-e0-probe-hid.md`.

**You verify after:**

Build + install as before. Verify Stage D 'A' injection still works (T05 regression check — easy to break by accident).

**Run the probe** (Window 1, non-elevated):
```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\x64\Debug\TestEnumeratorAndClient\TestEnumeratorAndClient.exe" --probe-hid
```

Expected output: enumeration finds the device, `CreateFile` returns INVALID_HANDLE_VALUE with `GetLastError() == 5` (`ERROR_ACCESS_DENIED`), formatted message "Access is denied."

The executor will append the result to `phase2-references.md` Section 5. Verify it did.

**Decision point — HARD STOP if unexpected:**
- If `CreateFile` returns SUCCESS, stop and bring me the result. The Stage E plan was designed assuming this fails. We may keep custom-interface anyway (architectural cleanliness) but I want to make that call.
- If enumeration finds zero matching devices, stop. Either kbdhid never attached (rerun T03 checks) or the probe's filter logic is wrong.

**Then:** commit T06.

---

## T07 — Stage E.1 driver: Custom device interface + injection IOCTL

**Hand to executor:** `tickets\T07-stage-e1-driver-custom-interface.md`.

**You verify after:**

Build + install as before.

**Verify the custom interface is published.** Window 2:
```powershell
$ourGuid = '{paste-guid-from-Public.h-here}'  # check Public.h for the actual GUID after T07
Get-CimInstance Win32_PnPSignedDriver | Where-Object { $_.DeviceID -like 'ROOT\TheSpikeyDriver*' } | Select-Object DeviceID, DeviceName

# Better check — list device interfaces:
$devcon = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Tools" -Filter devcon.exe -Recurse | Select-Object -First 1
& $devcon.FullName interfaces $ourGuid
```
You should see your device path listed.

**Verify Stage D regression-free:** Open Notepad, observe 'A' appears on cadence. (Wait — T07 changes the timer to NOT alternate 'A'; the timer just emits zeros. So Notepad should show **nothing** unless the IOCTL is exercised. That's expected.) Re-read T07's "Make ONLY these changes" point 6 if confused.

So the Stage D test now becomes: Notepad should show nothing for ≥10 seconds with no test app running. If 'A' is still appearing, T07 didn't disable the timer's alternation — flag it.

**Decision point:** If `devcon interfaces` doesn't list a device for the new GUID, T07's `WdfDeviceCreateDeviceInterface` call isn't taking. Don't start T08 — bring me the WPP trace from device add.

**Then:** commit T07.

---

## T08 — Stage E.1 test client: Press Enter, countdown, emit 'i'

**Hand to executor:** `tickets\T08-stage-e1-test-client-ux.md`.

**You verify after:**

Build (no install needed — driver unchanged from T07; you can skip Redeploy if you've installed since T07).

**The acceptance test (this is the spike):**
1. Open Notepad. Don't focus it yet.
2. Window 1: `& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\x64\Debug\TestEnumeratorAndClient\TestEnumeratorAndClient.exe"`
3. The terminal prompts `Press Enter to send 'i' in 5 seconds...`. Press Enter.
4. Click into Notepad to give it focus during the countdown.
5. Watch Notepad. After the countdown, exactly one `i` should appear.

**Reproducibility tests (required for spike acceptance):**

Reboot. Repeat steps 1–5. Same result.

`devcon disable` then `enable`:
```powershell
$devcon = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Tools" -Filter devcon.exe -Recurse | Select-Object -First 1
& $devcon.FullName disable 'root\TheSpikeyDriver'
& $devcon.FullName enable  'root\TheSpikeyDriver'
```
Repeat steps 1–5. Same result.

**Verify probe still works:**
```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\x64\Debug\TestEnumeratorAndClient\TestEnumeratorAndClient.exe" --probe-hid
```
Should still report ACCESS_DENIED (T06 behavior preserved).

**Decision point — SPIKE COMPLETE if all pass.** If 'i' doesn't appear:
- Wrong HID usage code in the IOCTL payload (should be `0x0C`, not `'i'` = `0x69`).
- IOCTL succeeded but `InjectPending` flag handling is broken in the timer.
- Custom interface not opened (test app got a different device).

Bring me the WPP trace showing whether the IOCTL arrived in the driver and what the timer did next.

**Then:** commit T08.

---

## T09 — Stage F: Hardening + evidence capture

**Hand to executor:** `tickets\T09-stage-f-hardening.md`.

**You do most of the work this ticket** (the executor just templates an evidence file). Fill in `driverspike/phase2-spike-evidence.md` as you do these:

1. **Clean reboot** → repeat T03 + T05 + T08 acceptance. All pass.
2. **`devcon disable/enable`** cycle → repeat T08. Pass.
3. **Full uninstall + reinstall:**
   ```powershell
   & "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1"
   ```
   The script removes old devices and driver-store entries before reinstalling. Confirm the reinstall succeeded (`Get-CimInstance` check from T02). Repeat T08 acceptance.
4. **Final removal** (you only do this if you want to fully tear down — for an ongoing spike, keep the driver installed):
   - `pnputil /enum-drivers` — note the OEM number for `TheSpikeyDriver.inf`.
   - `& $devcon.FullName remove 'root\TheSpikeyDriver'`
   - `pnputil /delete-driver oem<N>.inf /uninstall /force`
   - Re-run `pnputil /enum-drivers` — confirm the OEM is gone.

Capture all of this in `phase2-spike-evidence.md`.

**Decision point:** If anything regresses on reboot or after disable/enable, stop. New ticket needed.

**Then:** commit T09. Tag: `git tag phase2-spike-complete`.

---

## Things that are not on this list (for clarity)

- **No git pushing.** The spike is local until you decide otherwise.
- **No CI / no automated test runners.** Driver smoke tests are manual.
- **No `--no-verify` on commits.** If a hook complains, fix it.
- **No editing `phase1-*.md` files.** They are historical record.
- **No re-tagging `phase1-baseline`** even if you make small fixes — that tag is the rollback point.

## Rollback plan

If anything goes catastrophically wrong:

```powershell
# uninstall whatever's currently installed
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1" -WhatIf
# (review what it would do, then run for real if it'll uninstall correctly:)
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1"

# revert to phase 1 baseline
git checkout phase1-baseline -- .
```

If the test machine is in a state where the driver won't uninstall cleanly, **stop**. The project's stop rule (`orchestrator-prompt.md`) caps install retry at two attempts — beyond that, ask me before doing anything else to PnP state.
