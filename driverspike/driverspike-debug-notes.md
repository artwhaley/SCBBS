# Driverspike UMDF Keyboard Debug Notes

Last updated: 2026-05-05

## Goal

Build a virtual HID keyboard for the Star Commander MFD project while staying in UMDF if possible. The reason is distribution: moving to kernel mode likely means EV certificate / Partner Center / signing friction that is painful for a hobby project.

## Current High-Level State

The UMDF driver is functioning as a HID device and Windows recognizes the child collection as a keyboard, but completed keyboard input reports do not become real key events.

Observed repeatedly:

- Driver loads successfully.
- HID descriptor, device attributes, and report descriptor are queried and returned successfully.
- Windows creates a present HID keyboard child bound to Microsoft `kbdhid`.
- `IOCTL_HID_READ_REPORT` requests are posted repeatedly.
- We defer reads to a manual queue and complete them later from a timer.
- Completion buffers are 8 bytes.
- `WdfRequestSetInformation(Request, 8)` is set.
- Completed reports contain the intended keyboard bytes.
- No keypress appears.
- Caps Lock does not toggle.
- No LED/output report comes back down after Caps Lock.
- No unhandled startup IOCTLs appear in traces.

This strongly suggests the loss is after successful UMDF read completion and before `kbdclass` acts on the report.

## Important Project Files

- `TheSpikeyDriver/hid.c`
  - HID descriptor/report descriptor.
  - HID IOCTL dispatch.
  - Read completion helper.
  - Output/LED handlers.

- `TheSpikeyDriver/manualqueue.c`
  - Manual queue and Stage D timer injection.
  - Currently emits Caps Lock for diagnostics.

- `TheSpikeyDriver/TheSpikeyDriver.inf`
  - UMDF HID install path.
  - Modified to mirror a working GameTouch UMDF joystick package more closely.

- `driverspike-common.h`
  - Shared keyboard report and control structures.

- `phase2-plan.md`
  - Original phase plan.

## Stage D Timer Injection Status

Stage D was restored so the driver injects reports without requiring the user-mode client.

Current behavior:

1. First completed read emits an all-zero baseline report.
2. Later timer ticks alternate:
   - key down
   - all-zero release

We tested both:

- Lowercase `a`: `00 00 04 00 00 00 00 00`
- Caps Lock: `00 00 39 00 00 00 00 00`

Caps Lock is the current diagnostic because a successful class-driver path should produce an LED/output report back down.

Expected trace for current build:

```text
StageD manual queue+timer ready period=2000 ms firstDue=500 ms usage=0x39 modifier=0x00
StageD timer tick=2 ... bytes M=00 R=00 K=39 00 00 00 00 00
CompleteHidReadRequest ... parameterOut=8 parameterIn=0 need=8
CompleteHidReadRequest destination bytes (8): 00 00 39 00 00 00 00 00
CompleteHidReadRequest req=... info before=0 after=8
```

## 2026-05-13 Feature-Report Control Runtime Result

- Test signing remained enabled and Secure Boot was off.
- Build succeeded after switching the sideband path from the failed private WDF device interface to a vhidmini-style vendor HID feature-report collection.
- The first multi-collection descriptor failed to start because the keyboard collection had no Report ID while the vendor collection used Report ID 1.
- The follow-up build assigned Report ID 2 to the keyboard collection and kept Report ID 1 for the vendor control collection. The device then started.
- `--list-hid` showed the vendor control sibling: `VID=0xDEED PID=0xFEED UsagePage=0xFF00 Usage=0x0001 FeatureReportByteLength=60 [spikey-control]`.
- `--stats` worked through `HidD_GetFeature` and returned sane idle stats.
- `--inject 0x04` worked at the control/state-machine layer: `HidD_SetFeature` queued the command, stats showed pending key-down, and later stats returned to idle.
- WPP trace `spikey-feature-inject.txt` proved the driver emitted key-down `02 00 00 04 00 00 00 00 00` followed by key-up `02 00 00 00 00 00 00 00 00`; both completed with `STATUS_SUCCESS` and `Information=9`.
- After a relog, one delayed `--inject 0x04` produced real visible input in Notepad. It typed repeated `a` characters because the current implementation holds key-down until the next 2-second timer tick emits key-up, allowing normal Windows key repeat.
- Current conclusion: T02 control path is solved and T03 command emission is proven end-to-end through real Windows keyboard input. Remaining hardening: shorten the key-down/key-up interval so one command produces one keypress, then remove the temporary Num Lock fallback.

## 2026-05-12 Finish-Command-Injection Progress

### Commands Run

- `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd`
- `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1`
- `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\x64\Debug\TestEnumeratorAndClient.exe --stats`

### Observed Results

- Source work progressed through the ticket packet up to the point where runtime validation becomes the main task.
- Repeated `Debug|x64` builds succeeded after the T01-T04 code changes.
- A non-elevated redeploy attempt reported that deploy steps require an elevated PowerShell.
- A `--stats` attempt from this session reproduced `CreateFile err=31` on the control interface path before later code-only observability improvements were deployed.

### Session Constraints

- This session was not treated as a driver-signing or install-validation session.
- The keyboard subdevice had been intentionally disabled below the root device to avoid disruptive fallback typing during development.
- Because of those constraints, runtime diagnostics and behavioral claims were intentionally paused rather than forced.

### Current Code State

- T01: the temporary timer-driven bring-up indicator remains present, but the hardcoded diagnostic usage was switched from Caps Lock to Num Lock.
- T02: the control-interface open path now has better client-side and driver-side observability for the next real validation session.
- T03: injected commands now drive the manual-queue completion state machine in code, emitting key-down then zeroed key-up cycles while preserving the Num Lock fallback when idle.
- T04: stats semantics and client output now describe idle, pending-key-down, and pending-key-up states more clearly.

### Deferred Validation Matrix

- Idle behavior after install: not run in this session.
- Control path open via `--stats`: reproduced old failure before redeploy; post-change result not yet validated.
- Single injection / modified injection / repeat cycles: not run in this session.
- Reboot regression: not run in this session.
- Disable-enable or redeploy regression: not run in this session.

### Residual Risks / Open Questions

- The control-interface open failure is still unresolved until the updated build is redeployed and traced.
- The command-driven completion path now exists in code, but live down/up behavior and stuck-key safety remain unproven.
- The temporary Num Lock fallback has intentionally not been retired yet.

What we do not see:

```text
LED SetOutputReport byte=...
```

## Descriptor Tests Tried

### Original Boot Keyboard Descriptor

Initial report descriptor was a standard boot keyboard style:

- Usage Page Generic Desktop
- Usage Keyboard
- One application collection
- No Report ID
- 8-byte input report
- 1-byte LED output report

Windows queried it, accepted it, and posted reads, but no key input occurred.

### KMDF Sample Descriptor Shape

We then replaced the descriptor with the descriptor from a working KMDF HID keyboard minidriver walkthrough:

- `bcdHID = 0x0100`
- Input report remains 8 bytes, no report ID.
- Key array logical/usage max set to `0x00FF`.
- LED output remains 1 byte.

Trace confirmed:

```text
GET_REPORT_DESCRIPTOR len=69 appCollections=1 reportIdItems=0 head=05 01 09 06 A1 01 05 07
GET_REPORT_DESCRIPTOR keyboard contract hidVersion=0x0100 inputBytes=8 outputBytes=1 noReportId=1 descriptorTail=03 91 01 C0
```

Still no key input.

## IOCTL Startup Audit

Startup traces consistently show only:

| IOCTL | Behavior |
|---|---|
| `IOCTL_HID_GET_DEVICE_DESCRIPTOR` | Completes success, 9 bytes |
| `IOCTL_HID_GET_DEVICE_ATTRIBUTES` | Completes success, 32 bytes |
| `IOCTL_HID_GET_REPORT_DESCRIPTOR` | Completes success, descriptor length |
| `IOCTL_HID_READ_REPORT` | Deferred to manual queue, later completed success |

Not seen in startup traces:

- `IOCTL_HID_ACTIVATE_DEVICE`
- `IOCTL_HID_DEACTIVATE_DEVICE`
- `IOCTL_HID_GET_STRING`
- `IOCTL_HID_GET_INDEXED_STRING`
- `IOCTL_GET_PHYSICAL_DESCRIPTOR`
- `IOCTL_UMDF_HID_GET_INPUT_REPORT`
- `IOCTL_UMDF_HID_SET_OUTPUT_REPORT`
- unhandled IOCTLs

The counters stayed clean:

```text
notImpl=0 unhandled=0
```

Conclusion: for every IOCTL Windows actually sends during startup/read polling, we appear to answer correctly.

## Read Completion Paths Tried

### Direct Output Buffer Path

We first used:

- `WdfRequestRetrieveOutputBuffer`
- `RtlCopyMemory`
- `WdfRequestSetInformation(Request, 8)`
- `WdfRequestComplete(Request, STATUS_SUCCESS)`

Trace showed correct bytes in destination and `Information=8`.

### Sample-Style WDF Memory Path

Then we changed completion to mirror the sample helper style:

- `WdfRequestRetrieveOutputMemory`
- `WdfMemoryGetBuffer`
- `WdfMemoryCopyFromBuffer`
- `WdfRequestSetInformation(Request, 8)`

Trace showed:

```text
CompleteHidReadRequest req=... retrieveOutputMemory status=STATUS_SUCCESS parameterOut=8 parameterIn=0 need=8
CompleteHidReadRequest req=... outputMemoryBuffer=... len=8
CompleteHidReadRequest destination bytes (8): 00 00 39 00 00 00 00 00
CompleteHidReadRequest req=... info before=0 after=8
```

Still no key input and no LED output report.

## PnP Binding Evidence

The Spikey root device appears as:

```text
ROOT\HIDCLASS\0003
Service: mshidumdf
Children: HID\HIDCLASS\...
```

The live child device appears as a keyboard:

```text
Class: Keyboard
FriendlyName: HID Keyboard Device
Service: kbdhid
DriverInfPath: keyboard.inf
DriverInfSection: HID_Keyboard_Inst.NT
MatchingDeviceId: HID_DEVICE_SYSTEM_KEYBOARD
HardwareIds include: HID\VID_DEED&UP:0001_U:0006, HID_DEVICE_SYSTEM_KEYBOARD
```

So descriptor parsing and `kbdhid` binding are not the failure point.

## External Samples Investigated

Downloaded/extracted examples under `C:\tmp`:

- `C:\tmp\kmdf_step2`
- `C:\tmp\kmdf_minidrv_kbd_step2`

### KMDF + VHF Keyboard Walkthrough

Source:

- `C:\tmp\kmdf_step2\kmdf_sample\keyboard.c`

Key behavior:

- Creates VHF device with `VhfCreate`.
- Starts it with `VhfStart`.
- User client sends an 8-byte `KEYBOARD_REPORT`.
- Driver wraps it in `HID_XFER_PACKET`.
- Calls `VhfReadReportSubmit`.
- Sample reportedly types `hello`.

This is kernel mode.

### KMDF HID Minidriver Keyboard Walkthrough

Source:

- `C:\tmp\kmdf_minidrv_kbd_step2\kmdf_sample\queue.c`
- `C:\tmp\kmdf_minidrv_kbd_step2\kmdf_sample\timer.c`
- `C:\tmp\kmdf_minidrv_kbd_step2\kmdf_sample\driver.c`

Key behavior:

- KMDF HID minidriver.
- Uses `EvtIoInternalDeviceControl`.
- For `IOCTL_HID_READ_REPORT`, forwards request to a manual queue.
- Timer/client later retrieves pending read and completes it with an 8-byte keyboard report.
- Uses `RequestCopyFromBuffer` with `WdfRequestRetrieveOutputMemory` and `WdfMemoryCopyFromBuffer`.

We mirrored the descriptor and completion style. Still no UMDF key input.

Notable difference:

- KMDF sample is a kernel driver.
- It can create a KMDF control device with `WdfControlDeviceInitAllocate`.
- That exact sideband pattern is not available in UMDF.

## GameTouch UMDF Joystick Package Comparison

Investigated installed package:

```text
C:\Program Files (x86)\GameTouch Controller\GameTouch Server\GTc_UMDF_Joystick\driver\umdf2\x64\Release\DriverPackage
```

Files:

- `VhidminiUm.dll`
- `VhidminiUm.inf`
- `wudf.cat`

Live device:

```text
ROOT\HIDCLASS\0002
Name: GTc 128-Button Virtual Joystick
Service: mshidumdf
Child: HID\HIDCLASS\1&1731F3EA&0&0000
Child match: HID_DEVICE_SYSTEM_GAME
Usage: UP:0001_U:0004
Driver: input.inf / HID_Raw_Inst.NT
```

Conclusion:

- It proves UMDF HID game controller works on this machine.
- It does not prove UMDF keyboard input works.

INF comparison taught us:

- GameTouch uses `mshidumdf` / `WUDFRd`, UMDF 2.15.
- GameTouch sets:
  - `UmdfKernelModeClientPolicy=AllowKernelModeClients`
  - `UmdfFileObjectPolicy=AllowNullAndUnknownFileObjects`
  - `UmdfMethodNeitherAction=Copy`
  - `UmdfFsContextUsePolicy=CanUseFsContext2`
- Our live registry matched those values.

We tried two GameTouch-style INF parity changes:

1. Added Win10 `.NT.Filters` AddReg for `LowerFilters = WUDFRd`.
2. Changed UMDF DLL install path to `%12%\UMDF` and `ServiceBinary = "%12%\UMDF\TheSpikeyDriver.dll"`.

Build/deploy worked, but behavior did not change.

Current INF retains these parity changes.

The GameTouch DLL exposes strings:

```text
ReadFromRegistry
MyReportDescriptor
```

This suggests they may have a registry-backed descriptor override path. That could be useful for faster future descriptor experiments, but it did not reveal a keyboard-specific fix.

## Current Code Changes To Remember

## Command Runtime Model

The driver now treats command injection as a small two-phase lifecycle layered on
top of the existing timer/read-completion path:

1. Idle
   - `PendingEmitCount == 0`
   - no injected command is active
   - the temporary Num Lock sign-of-life fallback may still run
2. Command queued
   - `IOCTL_THESPIKEYDRIVER_INJECT_REPORT` stores modifier, keycodes, and remaining cycle count
   - the next command-driven completion should emit key-down
3. Key-down phase
   - the timer completes the next pending HID read using the stored modifier/keycodes
   - the command stays active and the next phase becomes key-up
4. Key-up phase
   - the timer completes the next pending HID read with an all-zero report
   - remaining cycle count is decremented
5. Completion
   - when remaining cycles reach zero, pending command fields are cleared
   - the driver returns to idle and may resume the temporary Num Lock fallback

### `manualqueue.c`

Currently uses Caps Lock:

```c
#define STAGE_D_KEYBOARD_USAGE_CAPS_LOCK 0x39u
#define STAGE_D_KEYBOARD_MODIFIER 0x00u
```

If returning to normal `a` injection, switch usage back to `0x04`.

### `hid.c`

Descriptor currently matches the KMDF sample shape:

- `bcdHID = 0x0100`
- Report descriptor length 69
- No report IDs
- 8-byte keyboard input
- 1-byte LED output

Read completion currently uses the sample-style WDF memory path and logs:

- request IOCTL
- parameter output/input lengths
- output memory pointer/length
- source bytes
- destination bytes
- information before/after

### `TheSpikeyDriver.inf`

Currently includes GameTouch-style Win10 parity:

```inf
[DestinationDirs]
DefaultDestDir = 13
UMDriverCopy = 12,UMDF

[TheSpikeyDriver_Inst_Win10.NT.Filters]
AddReg = TheSpikeyDriver_Win10_DeviceHw_AddReg

[TheSpikeyDriver_ServiceInstall]
ServiceBinary = "%12%\UMDF\TheSpikeyDriver.dll"
```

## Things That Did Not Fix It

- Restoring driver-side timer injection.
- Adding heavy logging.
- Sending `Ctrl+A`.
- Sending plain `a`.
- Sending Caps Lock.
- Adding first all-zero baseline report.
- Using no report ID.
- Matching KMDF sample descriptor.
- `bcdHID = 0x0100`.
- `bcdHID = 0x0111` previously.
- Direct output buffer copy.
- `WdfMemoryCopyFromBuffer` output memory copy.
- Returning success for activate/deactivate.
- GameTouch-style Win10 INF filter section.
- GameTouch-style `%12%\UMDF` service binary path.

## Current Best Theory

UMDF HID can expose a keyboard-looking child on this Windows path, and Windows will bind `kbdhid`, but `kbdhid/kbdclass` is not accepting the UMDF-completed `IOCTL_HID_READ_REPORT` as real keyboard input.

This may be:

- an undocumented limitation,
- a security boundary,
- a subtle UMDF HID/minidriver contract issue not visible in normal traces,
- or some missing kernel-side behavior provided by KMDF/VHF but not by `mshidumdf`.

The evidence does not support a simple bad report-byte or bad descriptor explanation.

## Recommended Next Steps

1. Ask outside experts before abandoning UMDF.
   - Best venue: OSR NTDEV.
   - Possible Reddit venue: `r/osdev`.

2. If continuing UMDF, gather lower-level evidence:
   - HIDClass/kbdhid ETW if available.
   - Device stack inspection with WinDbg/DevNode tools.
   - Compare IRP status/information at kernel boundary if possible.

3. If moving forward pragmatically, prototype KMDF/VHF locally:
   - Use the `C:\tmp\kmdf_step2` VHF sample as the closest "known typing" path.
   - Keep the user-mode client protocol from this repo.
   - Replace the UMDF HID minidriver side with VHF report submission.

4. Keep signing/distribution as a separate decision:
   - Kernel mode likely solves input behavior.
   - Kernel mode reintroduces EV/Partner Center/signing pain.

## Draft Reddit/Forum Post

Title:

```text
UMDF virtual HID keyboard: kbdhid binds and READ_REPORT completes, but no key events. Is UMDF keyboard injection possible?
```

Body:

```text
I’m working on a hobby Windows project that needs to generate keyboard input as a virtual HID device. We’re trying very hard to stay in UMDF because requiring an EV cert / kernel-mode distribution would make the project much harder to share.

What we have working:

- UMDF2 HID minidriver using mshidumdf / WUDFRd
- Root-enumerated HIDClass device
- Windows creates a child device that binds to Microsoft kbdhid
- Child matches HID_DEVICE_SYSTEM_KEYBOARD
- Boot-keyboard-style report descriptor, no report IDs
- 8-byte input reports: modifier, reserved, 6 keycodes
- IOCTL_HID_GET_DEVICE_DESCRIPTOR, GET_DEVICE_ATTRIBUTES, GET_REPORT_DESCRIPTOR all succeed
- IOCTL_HID_READ_REPORT is posted repeatedly by the stack
- We defer reads to a manual queue, then complete them successfully
- Completion buffer is 8 bytes, WdfRequestSetInformation(..., 8)
- We tried both direct output-buffer copy and WdfMemoryCopyFromBuffer
- Reports look correct, e.g. Caps Lock down: 00 00 39 00 00 00 00 00, then release: all zeroes

Where it breaks:

- No visible key input
- Caps Lock does not toggle
- No LED/output report comes back down to the driver
- No unhandled IOCTLs, no failed startup interrogation in our trace
- The device appears present and OK in PnP, with kbdhid bound

We compared against a working KMDF HID keyboard minidriver sample and a KMDF/VHF sample. Those work by completing pending HID reads or calling VhfReadReportSubmit. But both are kernel-mode. We also compared against a working UMDF virtual joystick package on the same machine; our INF/WUDF settings now closely match it, but keyboard reports still do not become key events.

Question: has anyone successfully generated real keyboard input from a UMDF virtual HID keyboard on Windows 10/11? Is there some missing IOCTL/status/descriptor detail, or is this a known boundary where kbdhid/kbdclass will bind but not actually consume UMDF-completed keyboard reports?

Any hints, working examples, or “don’t waste your time, use KMDF/VHF” confirmation would be hugely appreciated.
```

## 2026-05-13 Runtime Update: Real Input Proven

- Feature-report control collection is live and `--stats`/`--inject` work through `HidD_GetFeature`/`HidD_SetFeature`.
- After relog, one `--inject 0x04` produced visible lowercase `a` input through the Spikey keyboard child.
- The first successful command repeated many `a`s because the old timer held key-down until the next 2-second tick. The command release path was changed to a 50 ms key-down/key-up interval, and a post-relog test produced a single `a`.
- The Num Lock sign-of-life fallback has now been removed from the idle path. The idle timer still completes zero reports for diagnostics, but it no longer emits Num Lock.
- Current next gate: verify lowercase `i` (`usage 0x0C`) in Notepad, then test whether the same HID keyboard input reaches Star Citizen.
