# T04 - Client, Stats, And Injection Validation

## Goal

Polish the user-mode control path just enough to make validation dependable and to ensure driver stats reflect the real command lifecycle.

## Files In Scope

- `TheSpikeyDriver\hid.c`
- `TestEnumeratorAndClient\main.cpp`
- `driverspike-common.h`
- `driverspike-debug-notes.md`

Stop and ask before touching any other file.

## Current Problem

Even if injection now works, the operator still needs clean visibility into:

- whether the control path is openable
- whether a command was accepted
- what the driver thinks is pending
- what the last completed report looked like

The existing stats structure is close, but its semantics were created during the earlier spike.

## Make ONLY These Changes

1. In `hid.c`, make `GET_STATS` reflect the new command-driven behavior clearly.
   Requirements:
   - `PendingEmitCount` should mean remaining cycles, not merely original request count
   - `PendingModifier` and `PendingKeycodes` should reflect the active pending command state
   - idle state should be unambiguous

2. In `main.cpp`, make `--stats` and `--inject` output operator-friendly and aligned with the new semantics.
   Keep the CLI simple; do not invent a new command surface unless required.

3. If a tiny protocol cleanup is needed in `driverspike-common.h` for clarity, keep it backward-compatible and minimal.

4. Add a short section to `driverspike-debug-notes.md` documenting the new runtime model:
   - idle
   - command queued
   - key-down phase
   - key-up phase
   - completion

## Do NOT

- Do not change the core injection logic unless a small fix is required to make stats truthful.
- Do not add broad documentation unrelated to this feature.
- Do not add a new test harness.

## Acceptance Criteria

- `--stats` is useful before and after injection.
- `--inject` followed by `--stats` makes it obvious whether the command is pending or complete.
- Debug notes reflect the actual runtime model now in the driver.

## Suggested Validation

- Run `--stats` at idle.
- Run `--inject 0x04`.
- Run `--stats` again immediately and after completion.
- Verify output is understandable without reading the code.

## Report Back With

- Exact files touched.
- Any protocol-field meaning that changed.
- Whether the client behavior remained backward-compatible.
