# Phase 1 Tickets: UMDF2 Virtual HID Minidriver Spike (vhidmini2 parity)

Status target: complete tickets **in sequence** unless a ticket explicitly allows repeat (e.g. smoke test after fixes).

**Product note:** Long-term direction is a **keyboard-style emulator**; **this phase** stays aligned with **`vhidmini2`** (vendor usage, sample VID/PID)—**not** a boot keyboard or `kbdhid` stack. See `phase1-plan.md`.

## Scope lock

- **UMDF2** only.
- **x64** only (`Debug|x64`, `Release|x64`).
- Mirror **vhidmini2** for **install, enumeration, and the report/IOCTL surface the sample app uses**.
- No KMDF, no ARM64 in this phase.

## What the vhidmini2 sample is

- Virtual **HID minidriver**, **`HIDClass`**, root-enumerated (reference INF uses e.g. **`root\VhidminiUm`**).
- **Not** “a normal keyboard” in the Plug and Play sense—the sample uses **vendor-defined usage** (**usage page `0xFF00`**, **usage `0x01`**) and **VID/PID** checks in the app (`HidD_GetAttributes` + **`HidD_GetPreparsedData` / `HidP_GetCaps`**).

## Parity: copy for core function; skip accidental complexity

| Copy / match (needed for working smoke test) | Optional / later |
|-----------------------------------------------|------------------|
| Shared **`common.h`-class** report IDs, structs, sizes (via shared header) | Matching every WPP trace macro name to the sample |
| HID **report descriptor** and handler paths the **sample app** exercises | Extra CLI flags beyond a minimal pass/fail runner |
| **INF topology** from **`VhidminiUm.inx`** (`HIDClass`, **`MsHidUmdf`** + **`WUDFRD`**, service binary) | Pixel-perfect comment parity in INF |
| App: **`HidD_GetHidGuid`** + **CM** interface enumeration + **CreateFile** + attribute/caps filter + **`--list-hid` triage** | Custom device-interface GUID as primary discovery |
| (Optional non-parity escape hatch) compile-time custom interface for raw **`CreateFile`** debug — **only** if HID path blocked | Leaving escape hatch **on** for final acceptance |

Document any intentional deviation (e.g. different Hardware ID string) in ticket notes and keep **VID/PID / usage / descriptor / handler** agreement.

---

## Ticket 1 — Pre-flight (workspace and tools)

Description: confirm environment and paths before coding; **no git operations**.

Steps:

1. Confirm **`driverspike\driverspike.sln`** exists and loads **`TheSpikeyDriver`** and **`TestEnumeratorAndClient`**.
2. Confirm Visual Studio + **WDK** installed; intent is **UMDF2** / **x64** builds.
3. Confirm ability to run **elevated** **`pnputil`** / **`infverif`** on the test machine.

Tests:

1. Visual Studio opens the solution without missing-project errors.
2. `where msbuild` (or VS Developer Command Prompt) succeeds.

Acceptance criteria:

1. Ready to build driver + app locally without repo branching steps.

---

## Ticket 2 — Initialize spike solution (UMDF2 + one app)

Description: solution structure and platforms match the locked names.

Steps:

1. Solution path: **`driverspike\driverspike.sln`**.
2. Projects **exactly**: **`TheSpikeyDriver`** (UMDF2 driver), **`TestEnumeratorAndClient`** (console).
3. Solution configures **`Debug|x64`** and **`Release|x64`** only (no ARM64).
4. Do **not** add a KMDF project.

Tests:

1. `msbuild driverspike.sln /p:Configuration=Debug /p:Platform=x64` completes (or lists only expected compile gaps until later tickets).
2. NuGet restore is **not** a gate: native vcxproj solutions often have nothing to restore. If `/t:Restore` is a no-op or skipped, that is fine—**the build command above** is the real check.

Acceptance criteria:

1. x64-only configuration is intentional and documented.
2. Project names remain **`TheSpikeyDriver`** and **`TestEnumeratorAndClient`**.

---

## Ticket 3 — Shared protocol constants (sample `common.h` parity)

Description: one shared header used by driver and app, aligned with **vhidmini2** `common.h` for anything that crosses the wire.

Steps:

1. Add shared header (e.g. **`driverspike-common.h`**) in a single include location for both projects, containing:
   - control codes / report IDs / structures / size macros the sample uses
   - constants needed for app filtering if duplicated from sample
2. Include from **`TheSpikeyDriver`** and **`TestEnumeratorAndClient`**.
3. Match sample values for phase 1 (e.g. **`CONTROL_COLLECTION_REPORT_ID`**, **`TEST_COLLECTION_REPORT_ID`**, payloads).

Tests:

1. Grep for duplicated conflicting constants—there should be **one** definition per value.
2. Both projects compile with the shared header.

Acceptance criteria:

1. No diverging copies of report IDs, structs, or sizes between driver and app.

---

## Ticket 4 — UMDF2 driver entry + device add

Description: **`DriverEntry`**, **`EvtDeviceAdd`**, framework device and queues—aligned with sample flow.

Steps:

1. UMDF2 driver settings consistent with sample minidriver initialization.
2. Add-device path creates device object and queues; register callbacks on the same classes of requests the sample handles (feature / input / output / strings / caps as applicable).
3. **Remove the template's custom device interface GUID** from `Public.h` and remove any `WdfDeviceCreateDeviceInterface` call. Phase 1 enumerates via the standard HID interface class only — leaving a parallel custom GUID confuses triage.
4. Fail fast with clear status on unsupported paths where the sample does.

Tests:

1. Build **`TheSpikeyDriver`** **`Debug|x64`**.
2. `grep` confirms no custom interface GUID remains in driver sources.

Acceptance criteria:

1. Driver compiles; **`EvtDeviceAdd`** is real implementation, not a stub.
2. No custom device interface GUID published.

---

## Ticket 5 — HID report descriptor + attribute/query handlers

Description: descriptor and queries match sample behavior so **caps and attributes** filter correctly.

Steps:

1. Report descriptor and handlers for descriptor length, attributes (VID/PID), strings if sample app checks them.
2. Usage page / usage and report IDs consistent with **Ticket 3** and sample app expectations.

Tests:

1. Descriptor buffer lengths match declared sizes (no off-by-one vs sample).

Acceptance criteria:

1. Post-install, **`HidD_GetAttributes`** and preparsed **caps** can match the test app’s filters.

---

## Ticket 6 — Queue / request handlers (sample-visible API)

Description: implement handlers for **operations the sample app performs**, not minimal fakes. **Match vhidmini2 verbatim wherever possible** — this ticket is exactly where past attempts wandered off-spec and produced parallel bugs.

**Critical (UMDF2 — read before changing queue code):** In **`vhidmini.c`** **`QueueCreate`**, the **KMDF** build uses **`EvtIoInternalDeviceControl`**; the **UMDF** build uses **`EvtIoDeviceControl` only** (see sample comment: **`mshidumdf.sys`** remaps the path to **`IRP_MJ_DEVICE_CONTROL`**). **`TheSpikeyDriver`** must match the **UMDF** branch. **Do not** “upgrade” the driver to **`EvtIoInternalDeviceControl`**-only because kernel HID uses internal IOCTLs — that is **wrong for this sample’s UMDF path** and will drift from **`hid/vhidmini2/driver/umdf2`**.

Steps:

1. Configure the default queue with **`EvtIoDeviceControl`**, mirroring **`vhidmini.c`** **`QueueCreate`** under **`#else`** (UMDF). Do not invent a different topology.
2. Dispatch the same IOCTLs as the sample’s **`EvtIoDeviceControl`** switch for UMDF: **`IOCTL_HID_*`** for device/report descriptors, attributes, read/write report, strings, etc., and **`IOCTL_UMDF_HID_GET_FEATURE` / `SET_FEATURE` / `GET_INPUT_REPORT` / `SET_OUTPUT_REPORT`** where the sample uses them (not the KMDF-only **`IOCTL_HID_GET_FEATURE`** path in the **`#ifdef _KERNEL_MODE`** block). Same **success/failure shape** and **buffer sizes** as the sample—wrong length on “success” is a common cause of “app passed filter then hung or failed silently.”
3. Mirror the sample's file/function structure so debugging maps to the reference (e.g. a `hid.c` equivalent for HID IOCTL handling).
4. **WPP (or equivalent):** log each **IOCTL**, **input/output buffer lengths**, and **report ID** when the IOCTL carries one. Use this to compare against **vhidmini2** when something “works” but misbehaves.
5. Explicit `STATUS_*` behavior for unsupported IOCTLs—no silent traps.

**Prerequisite:** Do not treat Ticket 6 as “in progress” until the **install gate** in **Ticket 11** is met: **WPP** shows **`DriverEntry` + `EvtDeviceAdd`**, PnP **problem code 0** (see plan: hard gate before deep IOCTL work). If those are missing, you are not debugging IOCTLs—you are debugging INF / load / UMDF version.

Tests:

1. Sequences analogous to the sample test app do not crash or return spurious **not supported** for required paths.
2. WPP traces show **`EvtIoDeviceControl`** (UMDF handler) firing for the IOCTL codes the smoke path exercises, with enough detail to spot wrong buffer sizes or report IDs.

Acceptance criteria:

1. Driver surface matches **vhidmini2** app-visible behavior for the smoke sequence.
2. **UMDF:** HID traffic is handled on **`EvtIoDeviceControl`**, consistent with **`vhidmini.c`** `#else` / **`driver/umdf2`**. (KMDF sample uses **`EvtIoInternalDeviceControl`** — not applicable to this spike.)

---

## Ticket 7 — Install package: HIDClass UMDF minidriver ( **`VhidminiUm.inx`** pattern)

Description: **replace** any generic UMDF template INF with a package structurally like **`hid/vhidmini2/sys/VhidminiUm.inx`**—not a small edit to `System` class + `Root\Foo` + `WUDFRD` only.

**Explicit transformation from current template INF:**

| Current `TheSpikeyDriver.inf` | Replace with |
|---|---|
| `Class = System` | `Class = HIDClass` |
| `ClassGuid = {4d36e97d-e325-11ce-bfc1-08002be10318}` | `ClassGuid = {745a17a0-74d3-11d0-b6fe-00a0c90f57da}` |
| `Include = wudfrd.inf` (in `.NT`, `.NT.hw`, `.NT.Services`) | `Include = MsHidUmdf.inf` per `VhidminiUm.inx` |
| `Needs = WUDFRD.NT*` | `Needs = MsHidUmdf.NT*` (per `VhidminiUm.inx`) |
| `Root\TheSpikeyDriver` | Keep root-enumerated style; `Root\TheSpikeyDriver` is fine if app filters by VID/PID (it does). |
| `DriverVer = ;` (empty) | Leave for stampinf to fill. Do **not** hand-edit. |
| `$ARCH$`, `$UMDFVERSION$` tokens | Leave for stampinf — these are expected in the `.inx`. |

Steps:

1. Apply the transformation table above. Cross-check against `hid/vhidmini2/sys/VhidminiUm.inx` for section order and any items the table omits.
2. **`ServiceBinary`** points to built **`TheSpikeyDriver.dll`** at `%13%`.
3. **`.inx` vs `.inf`:** Prefer (a) — keep the file as `.inx`-style with stampinf tokens and let the project's stampinf/inf2cat steps produce the shipping `.inf`. Acceptable fallback (b): one fully expanded `.inf` with no `$` tokens.
4. Verify post-stampinf output has no unresolved `$ARCH$` / `$UMDFVERSION$` tokens before passing to `infverif` / `pnputil`.

Tests:

1. Run **`infverif`** on the **exact `.inf`** you install (current WDK syntax, e.g. `/w` for universal rules where applicable)—must report no errors.
2. `pnputil /add-driver ... /install` (or your org’s equivalent) succeeds.

Acceptance criteria:

1. Install is a **HIDClass virtual minidriver** path comparable to the sample—not a generic empty UMDF device INF.
2. App discovery uses **standard HID device interfaces** via **`HidD_GetHidGuid`** + SetupAPI—**not** a custom primary interface GUID you invented for enumeration.

---

## Ticket 8 — Build / artifact verification

Description: confirm **x64** outputs and paths for install commands.

Steps:

1. Build **`Debug|x64`** and **`Release|x64`**.
2. Record paths to **`.dll`**, **`.inf`**, app **`.exe`**.

Tests:

1. `msbuild driverspike.sln /p:Configuration=Debug /p:Platform=x64`
2. `msbuild driverspike.sln /p:Configuration=Release /p:Platform=x64`

Acceptance criteria:

1. Binaries exist at expected locations for smoke test.

---

## Ticket 9 — App: enumeration + filtering + test sequence (sample style)

Description: **`TestEnumeratorAndClient`** matches sample discovery and validation.

Steps:

1. **`HidD_GetHidGuid`** → **`CM_Get_Device_Interface_List_Size`** → **`CM_Get_Device_Interface_List`** → iterate → **`CreateFile`**.
2. **`HidD_GetAttributes`** (VID/PID) and **`HidD_GetPreparsedData`** + **`HidP_GetCaps`** (usage page / usage) like sample.
3. **Debug / triage mode (required):** a flag (e.g. `--list-hid` or `--verbose`) that prints **every** HID interface path enumerated with **VID/PID** and **usage page / usage** (and optionally device description). When strict filtering finds nothing, this proves whether the problem is “no HID interface published” vs “filter too strict” vs “wrong device.”
4. Run test operations using **Ticket 3** definitions (feature / input / output as sample does).
5. Lean CLI; non-zero exit on failure.
6. **Linker inputs:** add `hid.lib`, `setupapi.lib`, `cfgmgr32.lib` to project `AdditionalDependencies`. Without these, the agent will see unresolved-external errors and waste time treating it as a code problem. The vcxproj currently has no source files — add `main.cpp` (or equivalent) and configure it as a C++ console app.

Tests:

1. Before driver install: clear “no matching device” behavior.
2. After install: `--list-hid` shows your device (or shows it missing—actionable).
3. After install: strict mode opens target device and completes operations.

Acceptance criteria:

1. Enumeration and filter logic are **functionally equivalent** to the sample app approach (HID class GUID—not a custom discovery GUID for final pass).
2. Triage mode makes “silent filter miss” diagnosable in one run.

---

## Ticket 10 — Install + smoke test + remove

Description: end-to-end proof without requiring reboot (best effort).

**Critical:** `pnputil /add-driver /install` only stages and rebinds **existing** devices. Root-enumerated devices have no node yet — PnP will not create one. First-time install **requires `devcon install`** (or a custom SwDevice host). Right-click → Install on the `.inf` only stages; it does **not** create the root device node.

Steps:

1. Stage and create the device node:
   ```
   pnputil /add-driver TheSpikeyDriver.inf /install
   devcon install TheSpikeyDriver.inf root\TheSpikeyDriver
   ```
2. Verify **device started**, not just staged:
   - `Get-PnpDevice -InstanceId 'ROOT\TheSpikeyDriver\*'` shows status `OK`.
   - `Get-PnpDeviceProperty` / Device Manager: **problem code 0** — treat **10 / 31 / 39 / 52** as load/signing/policy/start failures before debugging IOCTLs (see plan).
   - `%windir%\inf\setupapi.dev.log` shows no errors for the install attempt.
3. Verify **HID interface publication:** run **`TestEnumeratorAndClient --list-hid`** (or equivalent). Confirm the **`HidD_GetHidGuid`** enumeration lists a path you can attribute to this device (then strict filter + tests). A driver can install and start yet still fail to expose a usable HID interface—this step catches that class of failure.
4. Run **`TestEnumeratorAndClient`** (full test); capture output.
5. Remove cleanly:
   ```
   devcon remove root\TheSpikeyDriver
   pnputil /delete-driver oem<N>.inf /uninstall /force
   ```
   (Capture `oem<N>.inf` from `pnputil /enum-drivers` after staging.)

Tests:

1. VID/PID and usage checks succeed in output.
2. Report operations succeed.
3. Install/remove complete without error.

Acceptance criteria:

1. Driver loads and responds on the sample-equivalent path.

---

## Ticket 6/7/9/10 iteration note

Tickets 6, 7, 9, and 10 form a **single iterative cluster**, not a sequential gate. You cannot fully validate driver request handlers without an installed driver, and you cannot validate the app's filter logic without a responding driver. Realistic loop:

1. Get a stub driver through Tickets 4–5, install via Ticket 7+10 to confirm load.
2. Iterate on Ticket 6 with reinstall cycles, using WPP traces (Ticket 11) to confirm IOCTL paths fire.
3. Bring up the app (Ticket 9) once enumeration succeeds.

Report status as one block for these four tickets, not four independent gates.

---

## Ticket 11 — Baseline diagnostics + triage instrumentation

Description: enough observability that any single failure is unambiguous **before** the Ticket 6/7/9/10 iteration loop begins. The previous attempt at this project failed because parallel bugs (driver startup + HID enumeration) couldn't be separated from each other — fix that here, up front.

Steps:

1. Driver WPP: init, device add, queue config, every internal IOCTL handler entry/exit, request failures.
2. App: interface path tried, error codes per `CreateFile` / `HidD_*` call, report sizes, pass-fail markers.
3. Set up trace capture **before** the first install attempt:
   - `tracelog.exe -start ... -guid ...` (or TraceView) attached to the driver's WPP control GUID.
   - **Blocking gate for Ticket 6:** after the **first successful install** (cluster with Ticket 7/10), capture evidence that **WPP shows `DriverEntry` and `EvtDeviceAdd`** and PnP shows **no problem code**. Do **not** expand Ticket 6 IOCTL logic until this gate passes (stub IOCTLs OK). Then confirm traces extend through **HID IOCTLs** as you flesh out Ticket 6.
4. Set up state probes:
   - PowerShell snippet: `Get-PnpDevice -InstanceId 'ROOT\TheSpikeyDriver\*' | Get-PnpDeviceProperty` for problem codes.
   - `%windir%\inf\setupapi.dev.log` tail after every install attempt.
   - `infverif` against the post-stampinf `.inf` before every install.

Tests:

1. Triage check: when a known-good build is broken on purpose (e.g. fail `EvtDeviceAdd` with a clear status), the failure is identifiable from traces + state probes within 60 seconds. If it isn't, fix instrumentation before continuing.

Acceptance criteria:

1. Any single failure during Ticket 6/7/9/10 iteration is attributable to one of: install rejection, driver load failure, IOCTL handler failure, or app filter failure — within 60 seconds, from logs alone. No mystery failures.

---

## Ticket 12 — Handoff and optional version-control milestone

Description: document the spike; **git use is optional** and only **after** the spike is accepted.

Steps:

1. Ensure **`phase1-plan.md`** and **`phase1-tickets.md`** stay consistent with what you built.
2. Optional: add **`driverspike/README.md`** with exact build/install/app commands and WDK/VS prerequisites.
3. Optional: if the team decides to **keep** the spike, create tag **`phase1-spike-baseline`** and/or commit—**not required** for intermediate failures.

Tests:

1. Another developer can run Tickets **1–10** from a clean tree using only the docs and solution.

Acceptance criteria:

1. Handoff is documented; VCS steps are explicitly optional.

---

## Ticket dependency order

**1 → 2 → 3 → 4 → 5 → 11 → [6 ⇄ 7 ⇄ 9 ⇄ 10] → 8 → 12**

Note the reordering vs the original linear list: **Ticket 11 (diagnostics) runs before the install loop**, and **Tickets 6, 7, 9, 10 are an iterative cluster** — see "Ticket 6/7/9/10 iteration note" above. Repeat the cluster as needed after behavioral changes.

---

## Hard stop criteria

1. **Ticket 7 / 10** fail **twice** with the **same** INF binary: **stop** — classify using **`setupapi.dev.log`**, **`infverif`**, PnP problem codes, signing/policy messages (**vs** transient devcon glitch **vs** duplicate hardware ID). Retry only after changing the **right** layer (INF syntax vs catalog/signing vs policy vs device conflict).
2. **Ticket 9** fails filtering: run **`--list-hid`** first — distinguish **no interface published** from **wrong VID/PID/usage**. Then align **usage page/usage**, **VID/PID**, report sizes / IDs vs **Ticket 3**.
3. Unsupported status on a path the **sample app** calls: fix **Ticket 6** before advancing.
4. **PnP problem code ≠ 0** after install: do **not** iterate IOCTL handlers — fix load/start/signing/INF/UMDF version alignment first (see plan).

---

## Execution agent delivery format

For each ticket:

1. Commands run  
2. Raw output or status lines  
3. Pass/fail and reason  
4. Artifact list  

---

## Final direction

1. Mirror what matters for **install + enumeration + communication**.  
2. **UMDF2 + x64** only for this phase.  
3. Do not build a “keyboard driver” product in phase 1—build **vhidmini2 parity** first (`phase1-plan.md`).
