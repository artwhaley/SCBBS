# Finish Command Injection Ticket Set

This ticket set finishes the UMDF keyboard spike by preserving a temporary timer-driven sign-of-life behavior during bring-up, switching that behavior from Caps Lock to Num Lock, and wiring the custom control path all the way through to HID read completion.

## Objective

Starting point:

- The driver already enumerates as a virtual keyboard.
- The driver already exposes a custom control interface and private IOCTLs.
- The test client already knows how to call `--stats`, `--inject`, and `--sweep`.
- The manual-queue timer path still emits a hardcoded Caps Lock pattern instead of consuming queued commands.
- The control interface currently appears present, but opening it from user mode has failed with Win32 error 31.

End state:

- Temporary bring-up behavior preserved early in the run, but changed from Caps Lock to Num Lock so the machine remains usable during repeated reloads.
- Autonomous timer-only test behavior removed only after the control path is validated.
- `TestEnumeratorAndClient.exe --stats` opens the control interface successfully.
- `TestEnumeratorAndClient.exe --inject <usage> [modifier]` causes the driver to emit a key-down report followed by a key-up report.
- `EmitCount` is honored as a number of down/up cycles.
- Driver stats reflect real pending and completed command state.

## Ticket Order

Execute in this order:

1. `T01-state-model-and-idle-behavior.md`
2. `T02-control-interface-open-path.md`
3. `T03-command-driven-report-emission.md`
4. `T04-client-stats-and-injection-validation.md`
5. `T05-hardening-and-evidence.md`

## Companion Files

Use these alongside the tickets:

- `OPERATOR-RUNBOOK.md` - what the human operator should do during the run
- `EXECUTOR-PROMPT-SCAFFOLD.md` - bounded worker prompt template for a single ticket
- `FINAL-CHECKLIST.md` - end-of-run verification checklist
- `orchestrator-prompt.md` - runbook for the orchestrator itself
- `TOMORROW-RUNBOOK.md` - exact runtime validation sequence for the next session
- `TOMORROW-KICKOFF-PROMPT.md` - prompt to start the next orchestrator/executor session
- `tools\Capture-SpikeyEvidence.ps1` - non-destructive evidence capture
- `tools\Set-SpikeyTestSigning.ps1` - small wrapper around test-signing `bcdedit` commands

## Global Guardrails

- Stay in UMDF. No KMDF, VHF, or kernel-mode detours.
- Do not change the HID descriptor unless a ticket explicitly says to.
- Do not remove the private IOCTL protocol unless a ticket explicitly says to.
- Do not broaden the public protocol structs unless a ticket explicitly says to.
- Do not run driver install commands more than twice for the same root cause.
- If runtime evidence contradicts a ticket's core assumption, stop and report before continuing.

## Key Files

- `TheSpikeyDriver\Device.c`
- `TheSpikeyDriver\Device.h`
- `TheSpikeyDriver\hid.c`
- `TheSpikeyDriver\manualqueue.c`
- `TheSpikeyDriver\manualqueue.h`
- `TestEnumeratorAndClient\main.cpp`
- `driverspike-common.h`
- `driverspike-debug-notes.md`

## Acceptance Summary

The whole packet is complete only when:

- The temporary Num Lock sign-of-life behavior is removed by the end of the packet.
- The driver sits idle without generating keystrokes on its own at final completion.
- The control interface opens successfully.
- Stats are retrievable at idle and after injection.
- Injected commands are translated into the expected HID keyboard reports.
- No stuck key or spontaneous key behavior remains after command completion.
