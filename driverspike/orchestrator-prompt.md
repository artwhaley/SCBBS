# Orchestration Prompt — Phase 1 UMDF HID Spike (vhidmini2 parity)

You are executing a strict phase from this repository. Use this prompt as your runbook; **do not expand scope** beyond `phase1-tickets.md` and `phase1-plan.md`.

## Objective

Work under:

- `driverspike\driverspike.sln`
- UMDF2 driver: **`TheSpikeyDriver`**
- Console app: **`TestEnumeratorAndClient`**

Deliver a **known-good UMDF2 virtual HID minidriver baseline** that **installs**, **enumerates via the standard HID device interface class**, and **responds** to the test client using the **same patterns** as `microsoft/Windows-driver-samples/tree/main/hid/vhidmini2`.

**Long-term product:** keyboard-style emulation is **out of scope** for this phase. Do **not** implement a boot keyboard stack, `kbdhid`-style device class, or “real keyboard” assumptions. Phase 1 = **vhidmini2 parity** (vendor usage **0xFF00** / **0x01**, sample filtering, report paths). Details: **`phase1-plan.md`**.

## Mandatory scope

1. **UMDF2** only (no KMDF, no kernel-mode additions).
2. **x64** only: `Debug|x64`, `Release|x64`.
3. Mirror sample behavior for **enumeration** and **driver request paths** the sample app uses.
4. No broad re-architecture beyond the tickets.
5. **Do not rename** `TheSpikeyDriver` or `TestEnumeratorAndClient`.

## Reference artifacts

- **`driverspike/phase1-tickets.md`** — primary checklist and acceptance criteria.
- **`driverspike/phase1-plan.md`** — INF/install philosophy, `.inx` vs `.inf`, parity scope, enumeration (**`HidD_GetHidGuid`** + CM interface list—**not** custom GUID-first discovery), **install failure classification**, **PnP problem codes**, **hard gate before deep IOCTL work**.

Execute tickets **in order**; document deviations and pause for confirmation.

## Bring-up discipline (do not skip)

- **Gate:** WPP must prove **`DriverEntry` + `EvtDeviceAdd`** and PnP **problem code 0** after install **before** treating Ticket 6 IOCTL depth as the problem domain (plan + Ticket 11 / 6).
- **Interfaces:** **Install ≠ HID path published.** Verify **`HidD_GetHidGuid`** enumeration + **`--list-hid`** (Ticket 9/10), not only “device exists.”
- **App:** Implement **`--list-hid`** (or equivalent) so filter misses are never silent.
- **Handlers:** Log **IOCTL + buffer lengths + report IDs**; wrong success shape breaks the app after enumeration passes.
- **Retries:** After **two** identical INF install failures, **classify** (INF vs signing/policy vs PnP conflict vs tool glitch)—see **Hard stop** in tickets.

## Required behavior (vhidmini2)

**App enumeration**

- `HidD_GetHidGuid`
- `CM_Get_Device_Interface_List_Size` / `CM_Get_Device_Interface_List`
- Iterate paths, `CreateFile`, validate candidate
- `HidD_GetAttributes` and `HidD_GetPreparsedData` + `HidP_GetCaps` before using the device

**Driver**

- Handlers for **feature get/set**, **input report get**, **output report set**, and **strings** if the sample client exercises them—not stubs that fail the smoke sequence.
- **Queue (UMDF2):** Register **`EvtIoDeviceControl`** as in **`vhidmini.c`** **`QueueCreate`** **`#else`** branch. **Do not** switch to **`EvtIoInternalDeviceControl`**-only “to match kernel HID” — that is the **KMDF** path, not the UMDF **`driver/umdf2`** sample this spike mirrors.

**Install package**

- Follow **`VhidminiUm.inx`** **intent**: **`HIDClass`**, **`MsHidUmdf`** + **`WUDFRD`**, root-enumerated hardware ID **pattern**, **`ServiceBinary`** → built **`TheSpikeyDriver.dll`**.
- Replacing a **generic template INF** requires a **full** HID minidriver package shape—not editing `System` class TODOs. See plan for **`.inx`/stampinf** vs fully expanded **`.inf`**.

## Anti-goals

- No KMDF, ARM64, or alternate enumeration scheme for the first pass.
- No skipping install + app smoke test.
- No git churn prescribed here; optional tag/README **after** success per **Ticket 12**.

## Stop rule for blockers

**Do not retry the same destructive driver install command more than twice.** If `pnputil /add-driver`, `devcon install`, or `devcon remove` fails twice with the same root cause, **stop and surface the failure to the user** with: command run, raw output, `setupapi.dev.log` excerpt, and `Get-PnpDeviceProperty` problem code. Driver-install retry loops are how test machines end up in inconsistent or unbootable states. The cost of pausing is low; the cost of a third blind retry can be high.

The same rule applies to any agent action that modifies system state (driver staging, service registration, registry edits) — two attempts, then stop and report.

## Deliverables for handoff

1. **`TheSpikeyDriver`** + **`TestEnumeratorAndClient`** sources building **x64**.
2. Working **HIDClass-style** INF (correct **stamp** / expansion—no dangling **`$` tokens** in what you install).
3. Evidence: **`infverif`** (or equivalent validation), **`pnputil`** install, app run (**VID/PID**, usage, report ops), clean remove.
4. Per-ticket evidence: commands, outputs, pass/fail, paths.
5. Optional: **`phase1-spike-baseline`** tag / **`driverspike/README.md`** per **Ticket 12** if the team keeps the spike.

## Execution order

Effective order: **1 → 2 → 3 → 4 → 5 → 11 → [6 ⇄ 7 ⇄ 9 ⇄ 10] → 8 → 12** (see `phase1-tickets.md`).

Tickets **6, 7, 9, 10** are an **iterative cluster**, not sequential gates — you cannot validate IOCTL handlers without an installed driver, or app filters without a responding driver. Report status for these four as one block. **Ticket 11 (diagnostics) runs before the cluster** so failures inside the loop are attributable in seconds, not hours.

If a ticket fails:

- Stop advancing.
- Fix only the failing layer.
- Re-run from the earliest ticket that retests that behavior.
- Apply the **stop rule** above on any install command that fails twice.

## Acceptance summary format (final report)

For each ticket:

1. Number / name  
2. Status  
3. Commands  
4. Output snippets proving pass  
5. Files touched  
6. Issues and fixes  

Prioritize a **verifiable** baseline over extra features.
