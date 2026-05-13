# Phase 2 Spike Plan — Keyboard Injector (vhidmini2 → Generic Desktop / Keyboard)

## Product direction vs this phase (read first)

- **Long-term goal:** keyboard-style emulator for Star Commander.
- **Phase 1 (done):** UMDF2 virtual HID minidriver with vendor-defined usage (`0xFF00`/`0x01`), vhidmini2 parity, `TestEnumeratorAndClient` proves install / enumerate / IOCTL round-trip.
- **This phase:** evolve the same minidriver into a device that the kernel keyboard stack (`kbdhid` + `kbdclass`) recognizes and consumes, then prove driver-originated input reports actually reach a focused window. **The spike is "done" when:** the test client launches, prompts the user to press Enter, runs a 5-second countdown (during which the user can change focus to Notepad), then sends a single `'i'` keystroke that appears in the focused window.
- **Out of scope this phase:** Star Commander integration, secure-input scenarios (UAC prompts, lockscreen), multi-device profiles, arbitrary IME / dead-key handling, NKRO / non-boot keyboard descriptors. We are proving the injection *path* is real, not building the product.
- **Boot keyboard is the intentional spike target.** Phase 2 commits to the 8-byte HID boot keyboard report (Usage Tables 1.4 §B.1). A product-grade NKRO descriptor with full media keys is a later phase — Phase 2 should not silently drift into it.

## Threat model & shipping stance

- This is a **dev-only spike** on test-signed Windows. We are not making any claims about secure desktop, anti-cheat compatibility, UAC injection, or kernel keyboard hooks.
- Synthetic-keyboard HID drivers are a normal driver-dev pattern. Microsoft's broad recommendation for application-level input simulation remains user-mode (`SendInput`, UI Automation) where it can meet the need; for hardware-level emulation where games consume Raw Input, virtual HID is the recognized and valid path.
- Third parties (anti-cheat, enterprise policies) treat virtual keyboards as high risk regardless. Beyond the test PC will need a separate distribution conversation (attestation, Partner Center, SmartScreen reputation). Not a Phase 2 concern; flagged so future-us doesn't forget.

## Critical invariants (must not drift)

These are global constraints that hold across all stages. Any drift means a stage's "passing" result is unreliable.

1. **Report ID = 0** (no report IDs in descriptor) — applies from **Stage C onward**. Therefore `InputReportByteLength == 8`, **not** 9. First byte of input report is the modifier byte, not a report ID. (Stage B is an intermediate where Report IDs still exist; the no-Report-ID restructuring is part of Stage C's descriptor swap.)
2. **HID usage codes**, not Virtual-Key codes or ASCII. `'A'` is HID usage `0x04`, not `0x41`. Modifier bits live in byte 0 (Left Shift = `0x02`, etc.).
3. **Single top-level collection.** Keyboard TLC only, throughout the spike. Stage E adds the control channel via a separate custom device interface (not a second TLC) — see Stage E.1.
4. **Driver-emitted state, not events.** A keyboard report is the *current* state of all keys. `kbdhid` infers events by diffing consecutive reports.
5. **First report after device start must be a known baseline (all-zeros).** Otherwise `kbdhid` may interpret garbage as stuck keys.
6. **Timer cadence ≥ 500 ms** during Stages D and E (prefer 1–2 s) to stay clear of UMDF read-pending timing windows.
7. **VS2022 / WDK 10.0 only.** VS2026 is on the box but not recommended for driver work yet.

## Reference material

- HID Usage Tables 1.4 §B.1 — boot keyboard report descriptor.
- `microsoft/Windows-driver-samples/hid/vhidmini2` — same minidriver scaffolding as phase 1.
- Microsoft docs: kbdhid attaches via the HID class driver spawning a child PDO with compatible ID `HID_DEVICE_SYSTEM_KEYBOARD`, matched by `input.inf`.

These should be checked into the spike's `phase2-references.md` as inline notes (descriptor byte breakdown, expected child PDO compatible ID, expected interface GUIDs) before Stage A is signed off.

## Guiding principle

**Change one variable at a time.** The phase 1 build is the known-good baseline. Every stage should leave the device installable and the test app able to talk to it — so when something breaks, the diff is small. Tag the working state as `phase1-baseline` and stash a clean signed build artifact aside before starting Stage B.

## Stages

### Stage A — Reference research (no code)

Pull these into `phase2-references.md`:

1. **Boot keyboard descriptor bytes — verbatim from HID Usage Tables 1.4 §B.1.** A second-model review supplied the candidate descriptor (Usage Page 0x01 / Usage 0x06, modifier byte, reserved byte, 5 LED bits + 3 padding output, 6× keycode array input). Lock the candidate, then *verify in HIDView* before any install. The common failure mode is off-by-one in report length, missing reserved byte, or wrong report count — copy the spec, parse it, then trust it.
2. **Confirm Report ID = 0 in the chosen descriptor.** No `0x85` (Report ID) opcode anywhere. `InputReportByteLength` should compute to exactly 8.
3. **Expected post-install state**: `kbdhid → mshidumdf → WUDFRd` in the device stack (verifiable via `devcon stack`); child PDO with compatible ID `HID_DEVICE_SYSTEM_KEYBOARD`.

**Deliverable:** `phase2-references.md` with descriptor bytes, expected stack, expected compatible ID, and a one-line statement of the kbdhid attachment mechanism.

### Stage B — Pre-flight: struct + descriptor length + caps consistency only

**Scope label:** This stage is *plumbing only*. We exercise the size-change machinery (descriptor length, `HID_DESCRIPTOR.wReportLength`, `common.h` structs, manual queue copy size). We **do not** exercise anything kbdhid cares about — the device stays on vendor usage page `0xFF00`. Do not expect any keyboard-stack behavior here.

- Bump `HIDMINI_INPUT_REPORT` to 8 bytes of payload; keep usage page `0xFF00`.
- Update `INPUT_REPORT_SIZE_CB` macro and the descriptor's input report size literal.
- Test client confirms `HidP_GetCaps` reports the new `InputReportByteLength`.
- Reinstall, smoke test exactly like phase 1.

**Acceptance:** test app round-trips IOCTLs; `HidP_GetCaps` reports new size; PnP problem code 0; no `setupapi.dev.log` errors.

If this breaks, the bug is in the size plumbing, not the keyboard stack.

### Stage C — Descriptor swap to Generic Desktop / Keyboard

Now flip the bytes that change device *identity*:

- Replace report descriptor with the boot-keyboard descriptor from `phase2-references.md` (Usage Page 0x01, Usage 0x06, single TLC, no Report IDs, 8-byte input, 1-byte LED output).
- Either drop the now-unused vendor structs (`HIDMINI_CONTROL_INFO`, feature report) **or** gate them behind `#ifdef KEYBOARD_MODE` so the test app keeps working through the riskiest transition. Lean toward the `#ifdef`.
- INF unchanged. VID/PID unchanged.

**Acceptance — debug hierarchy (check in this order, each one gates the next):**

1. **HIDView** parses the descriptor cleanly with usage page 1 / usage 6.
2. **Test app `HidP_GetCaps` returns** `UsagePage == 0x01`, `Usage == 0x06`, `InputReportByteLength == 8` (not 9), and exactly **one top-level collection**. This is one authoritative kbdhid-binding signal.
3. **kbdhid service binding (the *other* authoritative signal):**
   ```powershell
   Get-CimInstance Win32_PnPEntity |
     Where-Object { $_.Service -eq 'kbdhid' -and $_.PNPDeviceID -like '*VID_<ours>*' } |
     Select-Object Name, PNPDeviceID, Service, Status
   ```
   At least one row, `Status = OK`. **This proves kbdhid attached to our keyboard child PDO.** If empty, the descriptor isn't being consumed as a keyboard.
4. **Parent-stack sanity (NOT a kbdhid check):** `devcon stack root\TheSpikeyDriver` shows the UMDF stack (`mshidumdf` + `WUDFRd`). **`kbdhid` will NOT appear here** — it lives on the child PDO that `hidclass.sys` spawned. See `phase2-references.md` §3 for both expected stacks. Use this step only to confirm the UMDF side is healthy, not as a kbdhid signal.
5. **Optional child-stack inspection:** `devcon stack "HID\VID_<ours>*"` shows the child PDO with `kbdhid` as Controlling Service and `kbdclass` as Upper Filter. Useful for triage; redundant with step 3.
6. **WPP traces** show `EvtIoDeviceControl` IOCTLs arriving from kbdhid (not just our test app), at a reasonable cadence.
7. **PnP problem code 0**; no `setupapi.dev.log` errors.
8. **Reproduces after a reboot** *and* after `devcon restart root\TheSpikeyDriver` (UMDF lifetime ≠ boot lifetime; bugs often only appear on re-enumeration).

If step 3 fails, the issue is descriptor parsing — back to step 1. If step 3 passes but step 4 looks broken, the UMDF side itself is unhealthy and that's a different debug path.

### Stage C.5 — IOCTL stub posture for keyboard mode

Lock these down *before* Stage D so we're not debugging idle storms + LED storms + injection failures simultaneously.

- **`IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST`** — keep `STATUS_NOT_IMPLEMENTED`. Treated as resolved per second-model review: kbdhid handles this gracefully, simply notes the device doesn't support selective suspend, moves on. **Contingency:** if WPP traces show repeated failures or resets tied to idle IOCTLs after kbdhid attaches, escalate to "complete with `STATUS_SUCCESS` and no work." Do not invest in a real pending-IRP / power-callback pattern unless that's still not enough.
- **Output report (LEDs).** Implement a precise handler now:
  - Validate buffer length is exactly the LED report size (1 byte for our descriptor).
  - Ignore the contents (we don't actually have LEDs).
  - Complete with `STATUS_SUCCESS`.
  - Log via WPP at INFO level so we can confirm kbdhid is sending them.
- **`IOCTL_HID_ACTIVATE_DEVICE` / `IOCTL_HID_DEACTIVATE_DEVICE`** — leave `STATUS_NOT_IMPLEMENTED` for now; promote only if traces demand it.

**Acceptance:** install + reboot + 60-second idle. WPP shows no repeating-error storm. LED IOCTLs (if any arrive) complete success.

### Stage D — Driver-side injection from a hardcoded source

Before involving the test client, prove the report path with the dumbest possible producer. The existing periodic timer in `manualqueue.c` `TheSpikeyDriverEvtTimerFunc` already pulls the next pending read and completes it.

**Terminology note:** This is a UMDF *driver-side* timer running in `WUDFHost.exe` (user-mode driver host), not a kernel-mode timer. The behavioral effect — "injection happens with no test app involved" — is the same; the term matters for triage and debugger attach.

**Implementation:**
- On device start, **explicitly queue one all-zeros baseline report** before the timer is allowed to emit anything else (invariant #5).
- Maintain a `LastReport` in device context. The timer computes the next report, completes the pending read, and updates `LastReport`. Never emit a report identical to `LastReport`.
- Modify the timer to alternate between "press 'A' (HID usage `0x04`, Left Shift modifier `0x02`)" and "release all (all-zeros)" each tick. Slow it to 1–2 s (invariant #6) so we can see what's happening.
- **Add a WPP trace at WARNING level when the timer fires and no read request is available.** Without this, "sometimes types, sometimes doesn't" is unbearable to debug.

**Acceptance:**
1. WPP shows `IOCTL_HID_READ_REPORT` arriving from kbdhid at steady cadence.
2. Driver completes them; WPP confirms no "no read pending" warnings during steady state.
3. Notepad (focused) shows letters appearing on a 1–2s cadence, no test app running.
4. Reproduces after reboot **and** after `devcon restart`.
5. No stuck keys; releasing focus and refocusing doesn't surface a "phantom 'A' down" state.

If Notepad is empty but kbdhid is sending reads, the report content is wrong — most likely a VK/ASCII vs HID-usage mistake (invariant #2) or a Report-ID byte snuck in (invariant #1).

### Stage E — Wire the test client back in via custom device interface

**Committed mechanism: B-custom-interface.** kbdhid opens the keyboard TLC exclusively (`CreateFile` from user mode returns `ERROR_ACCESS_DENIED`), so the test app cannot use the HID interface as a control channel. Instead, the UMDF driver publishes a *separate* control endpoint via `WdfDeviceCreateDeviceInterface` using a custom device interface GUID. The test app opens this endpoint and sends standard `DeviceIoControl` payloads. The keyboard TLC stays kbdhid's exclusively; the descriptor stays frozen at Stage C's known-good bytes.

Phase 1 deliberately removed this scaffolding for vhidmini2 parity (phase1-plan §82). Re-introducing it here is the documented escape hatch and the architecturally cleanest separation between "be a keyboard" (HID stack) and "accept user-mode commands" (private IOCTL channel).

#### Stage E.0 — 5-minute confirmation that HID interface is exclusive

Before writing any new driver code, confirm the assumption that motivates the whole stage. After Stage D acceptance, with kbdhid attached and timer-driven injection working: from the test app, attempt `CreateFile` on the keyboard HID interface path, then attempt `HidD_SetOutputReport`.

| E.0 result | Action |
|---|---|
| `CreateFile` fails (`ERROR_ACCESS_DENIED` or similar) | Expected. Proceed to E.1. |
| `CreateFile` succeeds but `HidD_SetOutputReport` fails | Confirms exclusive ownership at IOCTL level. Proceed to E.1. |
| Both succeed | Unexpected. Pause and re-evaluate before E.1 — the working assumption is wrong, document and decide whether to keep custom-interface anyway (architectural cleanliness) or pivot. |

Record the exact `GetLastError()` value in `phase2-references.md` regardless of outcome — it documents the underlying behavior for future-us.

#### Stage E.1 — Custom device interface implementation

- Define a new `GUID` for the control interface in the shared header (e.g. `GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL`). Distinct from any phase 1 leftover GUID — start clean.
- In the UMDF driver's `EvtDeviceAdd`, call `WdfDeviceCreateDeviceInterface` to publish the GUID. (The phase 1 template-generated scaffolding for this is in git history; reference but do not blindly resurrect — write fresh against the current device add code.)
- Define a small set of private IOCTLs (e.g. `IOCTL_THESPIKEYDRIVER_TYPE_STRING`) using `CTL_CODE` with `FILE_DEVICE_UNKNOWN` and method buffered, in the `0x800–0xFFF` user-defined range.
- Driver's `EvtIoDeviceControl` switch (already exists for HID IOCTLs) gains cases for the custom IOCTLs. They stash the payload in `DeviceContext`.
- On receiving a "type this key" IOCTL, the driver emits a key-down report immediately (completing the next pending read with the keypress), then a key-up (all-zeros) report shortly after. For the spike, "shortly after" can be a fixed delay using a single-shot WDF timer or a second pending-read completion on the next periodic timer tick — whichever is simpler. Stage D's periodic timer can be disabled or kept idling once the IOCTL path works.
- Test app gains a second enumeration: `CM_Get_Device_Interface_List` against the custom GUID, `CreateFile` the result, `DeviceIoControl` to send the payload. Phase 1's HID enumeration code is no longer reused for the control channel — write a parallel small helper.
- **Test client UX (the spike's user-facing acceptance):** on launch, the app enumerates the custom interface and confirms it can open the device. It then prints `Press Enter to send 'i' in 5 seconds...` and waits for Enter. After Enter, it counts down `5... 4... 3... 2... 1... sending 'i'` (one second per number, giving the user time to focus a Notepad window). Then it sends one IOCTL with payload representing HID usage `0x0C` (the keyboard usage code for `'i'`) and exits. No CLI args required for this default mode.

**Acceptance:**
- Test app finds exactly one device exposing the custom interface GUID.
- `CreateFile` on it succeeds; `DeviceIoControl` returns success.
- Press-Enter-and-countdown sequence produces a single `'i'` character in the focused window.
- Reboot, re-run, reproduces.
- `devcon disable` / `enable`, re-run, reproduces.
- Stage D's hardcoded timer injection still works when the test app is *not* running (the keyboard channel must not depend on the control channel being open).

### Stage F — Hardening sweep

Only after E works end-to-end:

- Modifier handling (Shift/Ctrl), key-rollover edge cases.
- Decide whether the LED handler needs to actually report state back to user mode (probably not for this spike).
- Revisit idle handling only if Stage C.5 contingency was tripped.
- Clean removal: `devcon remove` / `pnputil /delete-driver` cycle does not destabilize.

## Tooling (VS2022 / WDK 17 only)

- Build: existing `Build-VS2022.cmd`.
- Descriptor inspection: `hidclient.exe` / `HIDView` from `C:\Program Files (x86)\Windows Kits\10\Tools\<ver>\x64\`.
- Verification (preferred order — `Get-PnpDevice` is missing or flaky on some shells):
  1. `devcon stack root\TheSpikeyDriver` — confirms `kbdhid → mshidumdf → WUDFRd` order.
  2. `Get-CimInstance Win32_PnPEntity | ? { $_.PNPDeviceID -like 'ROOT\TheSpikeyDriver*' } | Select Name, Service, Status, ConfigManagerErrorCode`.
  3. `devcon status root\TheSpikeyDriver`.
  4. `Get-PnpDevice -Class Keyboard` only as a convenience when it works.
- Re-enumeration test: `devcon disable root\TheSpikeyDriver` + `devcon enable root\TheSpikeyDriver` (or `devcon restart`).
- Same WPP / `tracelog` setup as phase 1; same `setupapi.dev.log` triage discipline.
- Same `pnputil /add-driver` + `devcon install root\TheSpikeyDriver` install sequence.

## Risks and mitigations

| Risk | Stage | Mitigation |
|---|---|---|
| Report descriptor parser rejects malformed bytes | C | HIDView before install; descriptor errors there are unambiguous |
| kbdhid attaches but reads never complete (UMDF queue timing) | D | Timer ≥ 500ms; WPP trace when no read is pending |
| First report carries garbage → stuck/phantom keys | D | Explicit zero-baseline first report on device start; `LastReport` diffing |
| Test app sends VK/ASCII instead of HID usage | D/E | Invariant #2 in critical-invariants block; usage table reference in `phase2-references.md` |
| kbdhid takes exclusive open of keyboard TLC | E.0 | Committed to B-custom-interface; E.0 is confirmation, not discovery |
| Custom interface GUID collides with a phase 1 leftover | E.1 | Define a fresh GUID; do not reuse the template-generated one |
| Test app's keyboard injection becomes coupled to control channel being open | E.1 | Acceptance requires Stage D timer injection still works with no test app running |
| Idle / LED notification storms mask real bugs | C.5 | Minimal handlers in place *before* Stage D, not after symptoms appear |
| Stack composition wrong (no kbdhid in stack) | C | `devcon stack` check is acceptance step 3, gates everything below |
| Reboot fixes / breaks behavior (lifecycle bug) | C/D/E | Acceptance criteria require both reboot and `devcon restart` reproduction |
| HVCI / blocklist policy update during the work | any | Already test-signed; don't change signing cert mid-stream |
| Spike silently drifts into "product keyboard" scope | any | Boot keyboard committed; new descriptors are a later phase |

## Known failure signatures

| Symptom | Likely cause | First check |
|---|---|---|
| Device installs but doesn't appear under Keyboards | Descriptor wrong (not parsed as keyboard TLC) | HIDView; then `HidP_GetCaps` |
| Appears as keyboard but no typing in Notepad | Report format wrong (Report ID byte / VK code / wrong usage) | WPP report dump; invariants #1, #2 |
| Typing intermittent | Queue/timing — timer firing with no pending read | WPP "no read pending" warning |
| Stuck or phantom keys | Missing zero baseline or no release report | Confirm baseline emitted on device start |
| Works after fresh install but breaks after reboot | UMDF/PnP lifecycle bug | `setupapi.dev.log` from boot; `devcon stack` post-reboot |
| Test app `CreateFile` on HID interface returns ACCESS_DENIED | Expected; confirms exclusive kbdhid ownership | Proceed to Stage E.1 |
| Test app cannot find custom interface device | Custom GUID not published, INF interface registration missing, or device not started | `Get-CimInstance Win32_PnPEntity`; check `WdfDeviceCreateDeviceInterface` return code in WPP |

## Definition of done (phase 2)

- Driver builds Debug|x64 (and Release if required).
- HIDView parses the descriptor cleanly.
- `HidP_GetCaps`: UsagePage 0x01, Usage 0x06, `InputReportByteLength == 8`, exactly 1 TLC throughout.
- Test app finds and opens a separate custom-interface-GUID endpoint; control commands flow through it.
- `devcon stack` shows `kbdhid → mshidumdf → WUDFRd`.
- Hardcoded driver-side timer injection (Stage D) produces observable keystrokes in a focused window with no test app running.
- `TestEnumeratorAndClient.exe` press-Enter-then-5s-countdown sequence produces a single `'i'` in a focused window (Stage E).
- All Stage D and E acceptance reproduces after a clean reboot **and** after `devcon disable` / `enable`.
- Driver stop/remove does not destabilize the system; reinstall reproduces the result.
