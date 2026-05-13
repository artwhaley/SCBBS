# T01 - State Model And Idle Behavior

## Goal

Preserve the existing sign-of-life timer behavior for now, but switch it from Caps Lock to Num Lock and refactor the state model so later tickets can add command-driven behavior cleanly.

This ticket does not need to make injection fully work yet, and it should not delete the only currently-useful visual sign-of-life indicator.

## Files In Scope

- `TheSpikeyDriver\Device.h`
- `TheSpikeyDriver\Device.c`
- `TheSpikeyDriver\manualqueue.c`
- `TheSpikeyDriver\manualqueue.h`

Stop and ask before touching any other file.

## Current Problem

The driver still reflects Stage D diagnostic behavior:

- hardcoded Caps Lock usage in the timer path
- alternating press/release driven by `TogglePressedState`
- timer logic that thinks in terms of synthetic toggling instead of command lifecycle

We do not want to remove the sign-of-life behavior yet, because repeated driver reloads still need an obvious indication that the HID read path is alive. But we do want to stop using Caps Lock for that signal.

## Make ONLY These Changes

1. In `Device.h`, add or repurpose state so the device context can eventually model command execution clearly, while keeping the existing timer-test behavior intact for now.
   Required direction:
   - do not remove support for the current alternating test signal in this ticket
   - prepare names or comments so later tickets can evolve the state cleanly

2. In `Device.c`, initialize any new or repurposed state explicitly.

3. In `manualqueue.c`, keep the alternating diagnostic logic for now, but change the hardcoded test usage from Caps Lock to Num Lock.

4. Update trace messages and nearby comments so they no longer describe Caps Lock when the code is now using Num Lock.

5. Preserve existing tracing where useful, but avoid broad state-machine rewrites in this ticket.

## Do NOT

- Do not change the HID report descriptor.
- Do not implement the final key-down/key-up completion flow yet if doing so forces changes outside this ticket's scope.
- Do not touch the test client in this ticket.
- Do not remove the timer object.
- Do not remove the timer-driven sign-of-life behavior in this ticket.

## Acceptance Criteria

- The driver builds.
- The sign-of-life test behavior still exists after this ticket.
- The hardcoded test usage in `manualqueue.c` is Num Lock, not Caps Lock.
- Traces and comments no longer incorrectly describe Caps Lock if the code now uses Num Lock.
- Any state changes made in `Device.h` are compatible with later command-path work.

## Suggested Validation

- Search the tree for Caps Lock-specific diagnostic text and confirm it is gone or intentionally replaced where this ticket touched code.
- Build `Debug|x64`.
- After deploy, the driver should still show the timer-driven sign-of-life behavior, but it should act through Num Lock rather than Caps Lock.

## Report Back With

- Exact files touched.
- Whether any old state fields were removed, renamed, or repurposed.
- Whether the Num Lock sign-of-life behavior still depends on the timer firing periodically.
