# Tomorrow Runbook - Finish Command Injection Runtime Session

This runbook is for the next real runtime validation session after the T01-T04 code work. The code has progressed, but runtime evidence is still intentionally incomplete.

## Current State

Known from the executor handoff:

- T01 code/build complete: fallback sign-of-life was switched from Caps Lock to Num Lock and remains intentionally present.
- T02 code/build partial: control-open diagnostics improved, but `--stats` is not proven after redeploy.
- T03 code/build partial: command-driven key-down/key-up state machine exists in code, but live behavior is not proven.
- T04 code/build partial: stats semantics are clearer, but live stats are not proven.
- T05 evidence is deferred.

Tomorrow's first real gate is not injection. The first gate is:

```text
updated build deployed + control interface opens + --stats returns
```

Do not treat injection behavior as meaningful until that gate passes.

## Physical Setup

Before starting:

1. Have a real keyboard available.
2. Open Notepad or another harmless text target for injection tests.
3. Keep an elevated PowerShell and a normal PowerShell open.
4. Expect Num Lock to flash while the fallback sign-of-life path is still enabled.
5. If any key appears stuck, disable or remove the Spikey device first, then investigate.

## Enter Driver Test Signing Mode

Run this from an elevated PowerShell:

```powershell
bcdedit /set testsigning on
bcdedit /enum | findstr /i "testsigning"
shutdown /r /t 0
```

Equivalent helper:

```powershell
cd "C:\Users\artwh\Downloads\Star Commander MFD\driverspike"
.\tickets\finish-command-injection\tools\Set-SpikeyTestSigning.ps1 -Enable -Restart
```

After reboot, verify from an elevated or normal PowerShell:

```powershell
bcdedit /enum | findstr /i "testsigning"
```

Expected:

```text
testsigning              Yes
```

If `bcdedit /set testsigning on` fails with a Secure Boot policy error, check:

```powershell
Confirm-SecureBootUEFI
```

If it returns `True`, test signing may be blocked by Secure Boot. Disable Secure Boot in firmware setup for this test session, then repeat the `bcdedit` command and reboot.

To leave test signing mode later:

```powershell
bcdedit /set testsigning off
shutdown /r /t 0
```

Equivalent helper:

```powershell
.\tickets\finish-command-injection\tools\Set-SpikeyTestSigning.ps1 -Disable -Restart
```

## Working Folder

Use this folder for all commands:

```powershell
cd "C:\Users\artwh\Downloads\Star Commander MFD\driverspike"
```

## Step 1 - Capture Pre-Build State

Normal PowerShell:

```powershell
.\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "pre-build"
```

This is non-destructive. It records signing state, PnP state, HID listing, and current `--stats` behavior if the client exists.

## Step 2 - Build

Normal PowerShell:

```powershell
.\Build-VS2022.cmd /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nr:true
```

Expected:

- `TheSpikeyDriver.dll` produced
- `TestEnumeratorAndClient.exe` produced
- `KeystrokeInjectorGui.exe` produced
- no build errors

If build fails, stop. Do not deploy an old build by accident.

## Step 3 - Deploy

Elevated PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
.\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1 -Configuration Debug -SkipBuild
```

Rules:

- If this fails once, read the output and classify the failure.
- If the same deploy failure happens twice, stop and bring the output back.
- Do not run blind repeated remove/install loops.

## Step 4 - Capture Post-Deploy State

Normal PowerShell:

```powershell
.\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "post-deploy"
```

Then run the binding check:

```powershell
.\TheSpikeyDriver\Validate-TheSpikeyDriverBinding.ps1
```

Expected:

- Root Spikey device present and healthy.
- Keyboard child present and bound to `kbdhid`.
- Num Lock fallback may be visible.

If Num Lock fallback is not visible, do not move to injection yet. First determine whether the HID read path is alive.

## Step 5 - Gate: Control Interface Open

Normal PowerShell:

```powershell
.\x64\Debug\TestEnumeratorAndClient.exe --stats
```

Expected:

- It lists the control interface path.
- One `CreateFile` attempt succeeds.
- `GET_STATS` succeeds.
- Stats print `state=idle` or another understandable command state.

If `CreateFile` still fails with error 31:

1. Run evidence capture:

   ```powershell
   .\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "control-open-failed"
   ```

2. Stop T03/T04 runtime testing.
3. Use the failure-specific prompt below.

## Step 6 - Gate: Single Inject

Only run this if `--stats` works.

Open Notepad and place focus where text can safely appear. Then normal PowerShell:

```powershell
.\x64\Debug\TestEnumeratorAndClient.exe --inject 0x04
```

Expected:

- One `a` appears, or trace/stats clearly show the command completed.
- Follow-up stats show idle or a completed command state.
- No key remains stuck.

Capture evidence:

```powershell
.\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "after-inject-a"
```

## Step 7 - Modified Inject

Only run this after single inject is safe.

Focus Notepad:

```powershell
.\x64\Debug\TestEnumeratorAndClient.exe --inject 0x04 0x02
```

Expected:

- Shifted `A` behavior if the focused target accepts the input.
- No stuck Shift state.

Capture evidence:

```powershell
.\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "after-inject-shift-a"
```

## Step 8 - GUI Smoke

Only after command-line inject works.

Start:

```powershell
.\x64\Debug\KeystrokeInjectorGui.exe
```

Use:

1. Enter `a`.
2. Click `Refresh Stats`.
3. Click `Start 10s Countdown`.
4. Focus Notepad before the countdown hits zero.
5. Read the diagnostics panel after injection.

The GUI is useful for interactive testing, but the console client is better for evidence because its output is easy to paste into notes.

## Step 9 - Reboot Regression

If the basic path works:

```powershell
shutdown /r /t 0
```

After reboot:

```powershell
cd "C:\Users\artwh\Downloads\Star Commander MFD\driverspike"
.\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "after-reboot"
.\x64\Debug\TestEnumeratorAndClient.exe --stats
.\x64\Debug\TestEnumeratorAndClient.exe --inject 0x04
```

## Step 10 - Disable/Enable Regression

Only if reboot regression is fine.

Elevated PowerShell:

```powershell
$devcon = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Tools" -Filter devcon.exe -Recurse | Select-Object -First 1
& $devcon.FullName disable 'root\TheSpikeyDriver'
& $devcon.FullName enable  'root\TheSpikeyDriver'
```

Then normal PowerShell:

```powershell
.\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "after-disable-enable"
.\x64\Debug\TestEnumeratorAndClient.exe --stats
.\x64\Debug\TestEnumeratorAndClient.exe --inject 0x04
```

## What To Do If Control Open Still Fails

Use this prompt with the next execution agent:

```text
We are resuming the driverspike finish-command-injection packet after a real deploy in test-signing mode.

Current blocker:
- T02 control interface open is still failing after redeploy.
- Do not work on injection behavior yet.
- Focus only on why CM enumerates the custom control interface path but CreateFile fails.

Read:
1. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\T02-control-interface-open-path.md
2. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\driverspike-debug-notes.md
3. The latest evidence folder under:
   C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\evidence

Allowed files:
- TheSpikeyDriver\Device.c
- TheSpikeyDriver\hid.c
- TheSpikeyDriver\Queue.c
- TheSpikeyDriver\Driver.c
- TheSpikeyDriver\Trace.h
- TestEnumeratorAndClient\main.cpp

Do not touch descriptor bytes, command state-machine logic, or GUI code.

Goal:
- Explain whether CreateFile reaches EvtDeviceFileCreate.
- If it does not, identify the stack/install/interface reason.
- If it does, identify what status or framework policy causes Win32 error 31.
- Make the smallest code change needed, or stop with a precise diagnosis if it is not a code issue.

Report:
- Files touched
- Evidence used
- Root-cause hypothesis
- Commands needed for validation
```

## What To Do If Inject Succeeds But No Key Appears

Use this prompt:

```text
We are resuming after T02 control-open success. --stats works and IOCTL_THESPIKEYDRIVER_INJECT_REPORT returns success, but the requested key does not appear.

Focus on T03 only.

Read:
1. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\T03-command-driven-report-emission.md
2. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\driverspike-debug-notes.md
3. Latest evidence folder under tickets\finish-command-injection\evidence

Allowed files:
- TheSpikeyDriver\Device.h
- TheSpikeyDriver\hid.c
- TheSpikeyDriver\manualqueue.c
- driverspike-common.h

Do not change the control interface open path unless evidence proves injection IOCTL is not reaching the driver.

Goal:
- Prove whether the command state is consumed by the manual queue.
- Prove whether key-down and zeroed key-up reports complete pending HID reads.
- Find any stuck state or report-shape mismatch.

Report the smallest fix or the precise failing layer.
```

## What To Do If A Key Gets Stuck

1. Stop typing into important windows.
2. Try the real keyboard key that appears stuck, then Esc.
3. Disable the device:

   ```powershell
   $devcon = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Tools" -Filter devcon.exe -Recurse | Select-Object -First 1
   & $devcon.FullName disable 'root\TheSpikeyDriver'
   ```

4. Capture evidence:

   ```powershell
   .\tickets\finish-command-injection\tools\Capture-SpikeyEvidence.ps1 -Tag "stuck-key"
   ```

5. Stop the session and debug release-report logic before further injection.

## End Of Session

When done, append a short outcome to `driverspike-debug-notes.md`:

- test signing state
- build result
- deploy result
- `--stats` result
- injection result
- stuck-key result
- whether Num Lock fallback remains

Then leave test signing mode only when you are done testing:

```powershell
bcdedit /set testsigning off
shutdown /r /t 0
```

Or:

```powershell
.\tickets\finish-command-injection\tools\Set-SpikeyTestSigning.ps1 -Disable -Restart
```
