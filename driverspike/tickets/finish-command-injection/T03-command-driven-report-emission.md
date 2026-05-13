# T03 - Command-Driven Report Emission

## Goal

Finish the last-mile wiring so `IOCTL_THESPIKEYDRIVER_INJECT_REPORT` drives the HID read-completion path, while deliberately preserving a fallback sign-of-life path until the control path is proven stable.

## Files In Scope

- `TheSpikeyDriver\Device.h`
- `TheSpikeyDriver\hid.c`
- `TheSpikeyDriver\manualqueue.c`
- `driverspike-common.h`

Stop and ask before touching any other file.

## Current Problem

The driver already accepts `IOCTL_THESPIKEYDRIVER_INJECT_REPORT` and stores:

- modifier
- keycodes
- emit count

But the manual-queue completion path does not consume that state. It still follows the old diagnostic model.

We want command-driven emission to start working in this ticket, but we do not want to delete the only sign-of-life mechanism until the control path has been validated end to end.

## Required Command Semantics

Treat one injected command as:

1. one key-down report using the pending modifier/keycodes
2. one all-zero key-up report

Interpret `EmitCount` as the number of down/up cycles to emit.

If a new command arrives while one is active, it is acceptable for this ticket to replace the old pending command, as long as that behavior is explicit and traced.

## Make ONLY These Changes

1. In `hid.c`, update `HandleInjectReportIoctl` so it:
   - validates input as today
   - stores modifier and keycodes
   - converts `EmitCount == 0` to `1`
   - marks the command active
   - sets the next phase to key-down
   - logs whether an in-flight command was replaced

2. In `manualqueue.c`, make the timer/manual-queue callback emit reports from the stored command state.
   Behavior:
   - if no command is active, it is acceptable in this ticket to preserve the existing Num Lock sign-of-life behavior
   - if no pending HID read exists, leave command state intact and return
   - if next phase is key-down, emit modifier/keycodes
   - if next phase is key-up, emit a zeroed report
   - after key-up, decrement remaining cycle count
   - if cycles remain, next phase becomes key-down
   - otherwise clear the active command state

3. Update trace messages so the emission sequence is easy to follow:
   - queued command
   - key-down emitted
   - key-up emitted
   - remaining cycles
   - command complete

4. Keep `CompleteHidReadRequest` and the HID read-forwarding path intact.

## Do NOT

- Do not add a multi-command queue in this ticket.
- Do not redesign the client protocol.
- Do not change the HID descriptor or control interface GUID.
- Do not remove the fallback Num Lock sign-of-life behavior in this ticket unless the ticket cannot be completed without doing so.
- Do not broaden `driverspike-common.h` unless truly required for compatibility.
  If you think a protocol change is necessary, stop and ask first.

## Acceptance Criteria

- A queued command can drive the next pending HID read completion.
- The driver emits a down report followed by a zeroed up report.
- `EmitCount` is honored as repeated down/up cycles.
- The command state returns to idle after completion.
- If no command is active, the fallback Num Lock sign-of-life behavior still exists unless a higher-level decision was made to remove it.

## Suggested Validation

- Build and deploy.
- Run `TestEnumeratorAndClient.exe --inject 0x04`.
- Confirm one `a` appears in a focused text field.
- Confirm no stuck key remains after the command completes.
- If possible, test an `EmitCount > 1` path as well.

## Report Back With

- Exact files touched.
- Whether new commands replace active commands or are rejected.
- Any assumptions made about pending read timing or command latency.
