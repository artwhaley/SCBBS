# Open Questions — Phase 2

Most original questions in this doc were resolved by second-model review and folded into [phase2-plan.md](phase2-plan.md) as concrete actions, invariants, or empirical tests. This file now tracks only what remains genuinely unknown or requires a human decision.

---

## Resolved (now living in the plan)

- **Q1 — Does kbdhid attach to a root-enumerated UMDF HID minidriver from the descriptor alone?**
  Resolved: **yes**, mechanism is `hidclass.sys` spawning a child PDO with compatible ID `HID_DEVICE_SYSTEM_KEYBOARD`, matched by `input.inf` to load `kbdhid` + `kbdclass`. Verified empirically in Stage C via `HidP_GetCaps` + `devcon stack` showing `kbdhid → mshidumdf → WUDFRd`.

- **Q2 — Once kbdhid attaches, does the HID interface remain openable from user mode?**
  Resolved: **no**, kbdhid opens the keyboard TLC exclusively (`ERROR_ACCESS_DENIED` on `CreateFile`). Plan default is now Path B; Stage E.0 is a 5-minute confirmation, not discovery.

- **Q3 — `IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST` posture.**
  Resolved: `STATUS_NOT_IMPLEMENTED` is permanent correct behavior. Cursor's escalation rule (to `STATUS_SUCCESS` no-work) preserved as a Stage C.5 contingency only if real-world traces disagree.

- **Q4 — Microsoft restrictions on root virtual keyboards.**
  Resolved: no OS-level blocklist on signed drivers. Reframed as a "Threat model & shipping stance" section in the plan. Real concern is anti-cheat / enterprise policy, deferred to post-Phase-2.

- **Q5 — Exact boot keyboard descriptor bytes.**
  Resolved: candidate descriptor supplied by second-model review. Folded into Stage A as a deliverable for `phase2-references.md`. Still must clear HIDView before any install (don't trust any model's bytes blindly).

---

## Resolved — additional

- **O1 — Path B mechanism: multi-TLC vs custom-interface?**
  Resolved: **B-custom-interface**, committed. Cross-model agreement (cursor, ChatGPT, Gemini all converge on this). Plan updated — Stage C descriptor stays single-TLC throughout; Stage E publishes a separate device interface GUID via `WdfDeviceCreateDeviceInterface` and exposes private IOCTLs.

---

## Open — empirical, resolves itself during the spike

### O2. Will Stage E.0 actually produce `ERROR_ACCESS_DENIED`?

**Status:** Working assumption is yes (Q2 above). 5-minute experiment in Stage E.0 confirms. No pre-work required; this resolves on its own. Listed here for completeness so it's not forgotten.

---

## Definition of done for this doc

- O2 closes via Stage E.0.

After O2 closes, this doc can be archived. No research-style questions remain blocking the plan.
