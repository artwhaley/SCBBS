# Phase 1 Spike Plan — UMDF2 Virtual HID Minidriver (vhidmini2 Parity)

## Product direction vs this phase (read first)

- **Long-term goal:** evolve toward a **keyboard-style emulator** for Star Commander.
- **This phase only:** ship a **UMDF2 virtual HID minidriver + one console test app** that matches **`microsoft/Windows-driver-samples/hid/vhidmini2`** closely enough to **install, enumerate, and exercise the same report paths** as the sample. No injection behavior yet—only prove load/install and a stable HID interaction surface.
- **Phase 1 is *not* a stepping stone to keyboard injection.** vhidmini2 uses vendor-defined usage (`0xFF00 / 0x01`), a custom report descriptor, and a user-mode test client that talks `HidD_*` directly. A keyboard requires Generic Desktop / Keyboard usage, a standard keyboard report descriptor, INF compatible IDs that bind **kbdclass / kbdhid** on top, and **no** user-mode client (the kernel keyboard stack consumes reports). Phase 1 only proves "a UMDF HID minidriver loads and responds" — the keyboard mutation in phase 2 will be split into two sub-spikes (descriptor/usage swap with the test app still attached, then dropping the app and verifying kbdhid binding + reports reaching a focused window).

**Execution agents:** Do **not** implement a "keyboard class driver," boot keyboard HID usage, or `kbdhid`-style stacks in phase 1. The reference sample is a **HIDClass virtual minidriver** with **vendor-defined usage** (`0xFF00` / `0x01`) and sample VID/PID—not a standard end-user keyboard device node. Keyboard-like behavior comes **after** this baseline is proven.

**Caveat on "known-good":** vhidmini2 is the closest published reference — not a guarantee. Non-WHQL UMDF HID minidrivers can be blocked by HVCI / Microsoft Vulnerable Driver Blocklist / codeintegrity policy on current Win11 builds. Test signing is already enabled on the test machine for this spike.

**Verify load, not just install:** `pnputil` / setupapi success does not prove the device **started**. After install, check Device Manager (or `Get-PnpDevice` / `DEVPKEY_Device_ProblemStatus`): **Code 0** = OK; **Code 52**, **Code 39**, **Code 31**, **Code 10** point to signing / policy / load / start failures — fix those before debugging IOCTLs or app filters.

**UMDF version alignment:** The **vcxproj** UMDF target / **`UmdfLibraryVersion`** (or equivalent) must agree with what **`$UMDFVERSION$`** expands to in the shipping **`.inf`** after stampinf. Mismatch is a common cause of “installed but won’t start.”

**Catalog / signing:** For test-signed packages, ensure the workflow you use produces a valid **`.cat`** (e.g. **inf2cat**) when required by your install path—INF syntax can pass **`infverif`** while the deployed package still fails policy checks.

## Goal

- Get a compileable UMDF2 driver and user-mode control app running, mirroring **vhidmini2** where it affects **install, enumeration, and communication**.
- Confirm the driver starts and the app can open the device and complete the **same style of report operations** the sample client performs.

## Reference implementation

- [Windows HID mini2 UMDF sample (vhidmini2)](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/vhidmini2)

## Deliverables (names are fixed)

A solution under `driverspike` containing:

- `driverspike.sln`
- **`TheSpikeyDriver`** — UMDF2 driver project (builds)
- **`TestEnumeratorAndClient`** — single C++ console app that enumerates HID interfaces, filters like the sample, and runs test I/O

Plus:

- Install package (**HIDClass / MsHidUmdf-style**, not a generic “System class + WUDFRD only” template—see [Install package](#install-package-full-replacement-not-a-tweak)).
- A small runtime proof: app finds the device via **`HidD_GetHidGuid`** + **SetupAPI interface list**, validates **VID/PID** and **usage page/usage**, then performs test report operations.

**Do not rename** `TheSpikeyDriver` or `TestEnumeratorAndClient` (orchestrator and tickets rely on these names).

## Architecture (phase 1)

- **One** app project handles enumeration + client tests (same process discovers `CreateFile` path, opens handle, sends/receives reports). Splitting into separate enumerator/client projects is **out of scope** for phase 1.

## File / module shape (mirror sample intent; match this repo’s language)

Mirror the **vhidmini2 UMDF** layout (driver entry, device add, queues, HID helpers). This repo’s template uses **C** sources (e.g. `Driver.c`, `Device.c`, `Queue.c`); the Microsoft sample may use **.cpp**—either is acceptable if the **WDF/HID callback structure** matches the sample. Prefer **same responsibilities per module** over identical filenames.

Suggested mapping:

| Concern | Align with sample module(s) | This spike (example) |
|--------|------------------------------|------------------------|
| Driver entry | Sample driver entry | `Driver.c` / `Driver.h` |
| Device add, callbacks | Sample device | `Device.c` / `Device.h` |
| Queues, IOCTL/report dispatch | Sample queue | `Queue.c` / `Queue.h` |
| Report descriptor + HID minidriver callbacks | Sample HID files | Add `hid.c` / `hid.h` (or split as sample does) |
| Shared constants with app | Sample `common.h` | Shared header (see tickets): values aligned with sample |

Add `Trace.h` / versioning helpers as needed to match the sample’s diagnostics pattern.

**App (`TestEnumeratorAndClient`):** `main` + helper(s) that implement the **same enumeration and validation sequence** as the sample test app (`HidD_GetHidGuid`, configuration-manager interface list, `CreateFile`, `HidD_GetAttributes`, preparsed data / caps). CLI flags are optional; **behavioral parity** matters more than extra switches.

## Parity scope (what to copy vs later)

Copy **verbatim or equivalent** for phase-1 success:

- **`common.h`-class constants:** report IDs, structures, sizes, and any defines the sample app and driver both use—via the shared header in tickets.
- **HID report descriptor** (and declared lengths): match the sample until the smoke test passes; diverging here breaks caps filtering and report sizes.
- **Driver handlers** on the **paths the sample app calls** (feature get/set, input report get, output report set, strings if the sample tests them): must return coherent success/failure—not stubs that leave the app stuck or failing caps/report checks.
- **INF package shape** for a **HIDClass UMDF virtual minidriver:** `HIDClass`, `MsHidUmdf` + `WUDFRD` needs/includes, root-enumerated hardware ID **pattern** as in **`VhidminiUm.inx`** (see below).

You **do not** need byte-for-byte parity with every optional sample file, comment, or WPP macro name. **Do** leave a short note in code or tickets when you intentionally differ (e.g. project display name only).

## Enumeration and device interfaces (avoid a common mistake)

- The app discovers devices using the **standard HID device interface class** from **`HidD_GetHidGuid`** and **`CM_Get_Device_Interface_List`**—same as the sample.
- **Post-install, prove interface publication:** The device can show as present in PnP yet still fail to expose a HID interface path your app can open. After each install, verify **`HidD_GetHidGuid` + `CM_Get_Device_Interface_List`** returns at least one **enumeratable path** for your device (and that **`CreateFile`** can obtain a handle—stub IOCTLs may still be required for a full handshake). This catches “silent” INF / MsHidUmdf / descriptor issues faster than **`infverif`** alone.
- Do **not** add a **custom device interface GUID** in the INF as the **primary** discovery mechanism for final parity validation. **HID class installation** exposes the interfaces the sample expects; focus INF work on **HIDClass + MsHidUmdf** patterns from **`VhidminiUm.inx`**.
- The Visual Studio template generated a **custom device interface GUID** in `Public.h` and registers it via `WdfDeviceCreateDeviceInterface`. **Remove both** for **strict parity** — leaving them in publishes a parallel interface that confuses triage when greps find two GUIDs.
- **Optional escape hatch (non-parity):** If HID enumeration is blocked and you need a binary **CreateFile** target to separate “INF/load” from “HID stack,” you may temporarily reintroduce the template-style custom interface behind a **compile-time flag** (default **OFF**), document it in ticket evidence, and **do not** treat that path as passing acceptance criteria. Prefer **WPP + “list all HID devices” app mode + PnP problem codes** first — a second interface doubles INF/driver surface and can hide HIDClass bugs.

## HID minidriver request handling (UMDF vs KMDF — do not “helpfully” swap callbacks)

The shared **`vhidmini2`** sources (`hid/vhidmini2/driver/vhidmini.c`, **`QueueCreate`**) compile for **both** KMDF and UMDF. The **queue callback differs by model**:

| Model | Queue registration | Why |
|--------|---------------------|-----|
| **KMDF** (`#ifdef _KERNEL_MODE`) | **`EvtIoInternalDeviceControl`** | HIDClass sends **`IRP_MJ_INTERNAL_DEVICE_CONTROL`** with **`IOCTL_HID_*`**. |
| **UMDF2** (`#else`) | **`EvtIoDeviceControl` only** | Sample comment: HIDClass uses internal IOCTLs that UMDF does not surface the same way; **`mshidumdf.sys`** converts the path so the UMDF driver receives **`IRP_MJ_DEVICE_CONTROL`** (including **`IOCTL_HID_*`** for some IOCTLs and **`IOCTL_UMDF_HID_*`** for feature/input/output **`HID_XFER_PACKET`** paths). |

**Phase 1 (`TheSpikeyDriver`) is UMDF2.** Mirror the **`#else`** branch: register **`EvtIoDeviceControl`** and dispatch from the same **`EvtIoDeviceControl`** switch as the sample (do **not** replace it with **`EvtIoInternalDeviceControl`**-only wiring “because HID is internal IOCTL” — that would **diverge** from **`driver/umdf2`** parity and mislead future agents).

**Guardrail for automation:** Do not “fix” working UMDF code by switching the default queue to **`EvtIoInternalDeviceControl`** without proving a regression against **`vhidmini.c`**; the published UMDF path is **`EvtIoDeviceControl`**.

- Match the sample’s **IOCTL set and completion semantics** (descriptor, attributes, report descriptor, read/write report, strings, **`IOCTL_UMDF_HID_*`** feature/input/output, idle/activate stubs as in sample **`default`** branch).
- Do **not** invent your own queue topology or handler shape. Match **`vhidmini.c` / `umdf2/util.c`** so debugging maps to the reference.

## Install package: full replacement, not a tweak

The Visual Studio "UMDF driver" template INF (**wrong class**, **`Root\YourDevice`**, **`System`**, **`WUDFRD` only**) is **not** the same as the **vhidmini2 UMDF HID minidriver** package.

For phase 1 you **replace** that layout with an install package that follows **`hid/vhidmini2/sys/VhidminiUm.inx`** (processed) **intent**:

- **`Class = HIDClass`** with **ClassGuid `{745a17a0-74d3-11d0-b6fe-00a0c90f57da}`** — not `System` / `{4d36e97d-...}`.
- **`Include` / `Needs`** from **`MsHidUmdf.inf`** (replaces the template's `wudfrd.inf` references) — order and sections matter; copy from `VhidminiUm.inx`.
- **Hardware ID** in the **root-enumerated** style the sample uses (e.g. `root\VhidminiUm` in the reference INF)—your spike may keep the sample hardware ID or use a distinct ID **only if** INF, device strings, and app filters stay consistent.
- Service install pointing at your built **`TheSpikeyDriver.dll`** in **`ServiceBinary`** (`%13%\TheSpikeyDriver.dll`).
- Leave **stampinf** to substitute `$ARCH$`, `$UMDFVERSION$`, `DriverVer` — these tokens are **expected** in the `.inx`. Do **not** hand-edit them out.

Treat INF work as **adopting the sample's INF topology**, not editing a few `TODO` lines on the generic template.

## `.inx` vs checked-in `.inf` and stamp variables

The published sample ships **`VhidminiUm.inx`**, which the WDK build typically processes with **stampinf** (e.g. **`$ARCH$`**, **`$UMDFVERSION$`**, **`DriverVer`**, catalog naming).

For this spike, choose **one** approach and document it in the ticket evidence:

1. **Preferred for parity:** Add an **`.inx`** (or equivalent) and let the **driver project’s inf2cat/stampinf** steps produce the shipping **`.inf`**—same workflow as the sample build.
2. **Acceptable:** Maintain a **single expanded `.inf`** with concrete **x64** strings and a real **`DriverVer`**, generated once from the sample pattern—**no unreplaced `$ARCH$` / `$UMDFVERSION$` tokens** in what you pass to **`pnputil`** / **`infverif`**.

Do not hand-copy a half-expanded `.inx` with placeholders still in place.

## Version control (this spike)

- **No git workflow is prescribed here** (no branch creation, push, or mandatory commits while validating the spike).
- The team may keep a **local checkpoint**; **record results** (build logs, `infverif`, smoke run) as ticket evidence. Optional commit/tag **after** the spike is verified is described in the last ticket only.

## Environment + prerequisites

1. Visual Studio with C++ desktop workload + **Windows Driver Kit** matching the VS generation.
2. Test signing is already enabled on the test machine — no action required.
3. Elevated sessions for **`pnputil`**, **`infverif`**, and driver install/remove.
4. **`devcon.exe`** from the **same WDK build** as your tools (match **`<arch>`** to **x64** on x64 OS — wrong-arch devcon causes confusing failures). Typical path: `C:\Program Files (x86)\Windows Kits\10\Tools\<ver>\x64\devcon.exe`. **Required** for first-time install of a root-enumerated device: `pnputil /add-driver` alone will not create the device node. See "Install command sequence" below.

## Install command sequence (root-enumerated HID minidriver)

`pnputil /add-driver /install` does **not** create root-enumerated devices — it only stages and rebinds existing devices. For first install:

```
pnputil /add-driver TheSpikeyDriver.inf /install
devcon install TheSpikeyDriver.inf root\TheSpikeyDriver
```

Right-click → Install on the `.inf` is **not equivalent** — it stages the package but does not create the root device node. Subsequent updates (after the device exists) can use `pnputil /add-driver /install` for rebinding.

Removal:

```
devcon remove root\TheSpikeyDriver
pnputil /delete-driver oem<N>.inf /uninstall /force
```

Capture the OEM number from `pnputil /enum-drivers` after staging.

## Build validation

- Primary gate: **`msbuild driverspike.sln /p:Configuration=Debug /p:Platform=x64`** (and Release when needed).
- **NuGet `/t:Restore`** is **not** required for typical native WDK vcxproj solutions; use it only if the solution actually uses SDK-style package restore.

## Phase 1 implementation steps (summary)

1. Ensure solution contains **`TheSpikeyDriver`** + **`TestEnumeratorAndClient`**, **x64** only for phase 1.
2. Add shared protocol header aligned with sample **`common.h`** (report IDs, structs, sizes).
3. Implement driver **DriverEntry** / **EvtDeviceAdd**, default queue with **`EvtIoDeviceControl`** (UMDF **`vhidmini.c`** `#else` branch — **not** `EvtIoInternalDeviceControl`-only), and HID IOCTL handlers matching the sample's exercised paths. Remove the template's custom interface GUID + `WdfDeviceCreateDeviceInterface` call.
4. Replace INF with **HIDClass UMDF minidriver** package pattern from **`VhidminiUm.inx`**, with clear **.inx vs .inf** handling.
5. Implement app enumeration + filtering + test sequence like the sample client (link `hid.lib`, `setupapi.lib`, `cfgmgr32.lib`).
6. Install via `pnputil /add-driver` + `devcon install`, smoke test, remove via `devcon remove` + `pnputil /delete-driver`; capture logs.

## Realistic execution loop (tickets are not strictly sequential)

Tickets 6 (driver IOCTL handlers), 7 (INF), 9 (app), and 10 (install + smoke) form a **single iterative cluster**. You cannot fully validate driver request handlers without an installed driver, and you cannot validate the app's filter logic without a responding driver. Expect to:

1. Get a stub driver (Tickets 4–5) to install (Ticket 7 + 10) before fleshing out IOCTLs.
2. Iterate on Ticket 6 with reinstall cycles, watching WPP traces.
3. Bring up the app (Ticket 9) once the driver enumerates.

Treat the Ticket 6/7/9/10 loop as one block in your status reporting.

## Triage instrumentation (avoid the parallel-bug trap from last attempt)

Before starting Ticket 6/7/10 iteration, set up enough observability that any single failure is unambiguous:

- **WPP trace capture:** `tracelog.exe` or TraceView attached to the driver's WPP GUID before install. Confirm `DriverEntry` and `EvtDeviceAdd` traces appear in a known-good baseline run.
- **Hard gate before deep IOCTL work (Ticket 6):** Do not spend time debugging full HID IOCTL handlers until you have **one successful install** where **WPP proves `DriverEntry` + `EvtDeviceAdd`** and **PnP shows no start error** (problem code **0**). If those traces never appear, the failure is **not** your IOCTL switch statement—it is install, **INF / MsHidUmdf composition**, signing, or **UMDF version** mismatch. Stub handlers that return `STATUS_NOT_SUPPORTED` are fine until this gate passes.
- **Device state probe:** PowerShell `Get-PnpDevice -InstanceId 'ROOT\TheSpikeyDriver\*' | Get-PnpDeviceProperty` (or Device Manager → device → Properties → Events) to read `DEVPKEY_Device_ProblemStatus` / `DEVPKEY_Device_DriverProblem`.
- **Install-time errors:** capture `%windir%\inf\setupapi.dev.log` after each install attempt — this is the authoritative source for INF rejection / signing failures.
- **infverif** before every install attempt against the *exact* `.inf` you're about to install (post-stampinf).

When something fails, the question "did it fail to install, fail to load, or fail to respond?" must be answerable in under 60 seconds from these four sources. If it isn't, **fix the instrumentation first** before the next code change.

**Install retry discipline:** Blind repeats waste time. After **two** failed install/start attempts on the **same** INF binary, **stop** and **classify** the failure using `setupapi.dev.log`, **`infverif`**, PnP problem codes, and (if applicable) code integrity / signing messages — then fix the **category** (INF syntax vs signing/policy vs duplicate device vs transient tool glitch). Do not “try once more” without classification.

**Handler validation (Ticket 6):** Matching the IOCTL **list** is insufficient. Log **IOCTL code**, **buffer lengths**, and **report ID** (where applicable) and compare against **vhidmini2** behavior—“success but wrong length” passes enumeration then fails mysteriously in the app.

## Acceptance criteria (before phase 2)

- Driver builds **Debug|x64** (and Release if required).
- INF passes **`infverif`** and installs; device appears under **HID** and starts.
- App enumerates via **HID GUID + CM interface list**, matches **VID/PID** and **usage page/usage**, opens handle, completes sample-style report operations.
- Driver stop/remove does not destabilize the system.

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| INF class / includes wrong | Follow **`VhidminiUm.inx`** sections; compare side-by-side with the repo sample. |
| Descriptor vs caps mismatch | Keep descriptor and lengths aligned with sample until smoke passes. |
| App opens wrong device | Filter like sample app (**attributes + preparsed caps**), not path guessing. |
| Unexpanded INF tokens | Stampinf or fully expanded `.inf` before install. |

## Definition of done (phase 1)

- **`TheSpikeyDriver`** + **`TestEnumeratorAndClient`** build and run against a **vhidmini2-aligned** install and protocol surface.
- Optional version-control milestone (tag/README) only **after** verification, per tickets—not required for day-to-day spike work.
