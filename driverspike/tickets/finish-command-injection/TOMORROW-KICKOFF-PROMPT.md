# Tomorrow Kickoff Prompt

Use this when you start the next execution/orchestration session.

```text
We are resuming the driverspike UMDF keyboard finish-command-injection stack for a real runtime validation session.

Important current state:
- Code progressed through T01-T04, but runtime validation is still incomplete.
- T01 switched the fallback sign-of-life behavior from Caps Lock to Num Lock and intentionally kept it alive.
- T02 added better control-interface open diagnostics, but --stats is not yet proven after a proper redeploy.
- T03 added command-driven key-down/key-up report emission in code, but live injection is not yet proven.
- T04 clarified stats semantics and client output.
- T05 evidence is deferred.

Read these first:
1. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\TOMORROW-RUNBOOK.md
2. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\driverspike-debug-notes.md
3. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\README.md
4. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\T02-control-interface-open-path.md
5. C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\T03-command-driven-report-emission.md

Mission for this session:
- Do not start by changing code.
- First restore the proper test-signing/deploy conditions.
- Build the current code.
- Redeploy from an elevated PowerShell.
- Capture evidence.
- Gate on --stats. If --stats cannot open the control interface, stop injection testing and focus only on T02.
- Only if --stats works, test --inject.

Guardrails:
- Two deploy attempts maximum for the same root cause.
- Do not touch HID descriptor bytes unless evidence specifically forces that discussion.
- Do not remove the Num Lock fallback until command injection is proven.
- Stop on stuck-key behavior or spontaneous typing that is not the intentional Num Lock fallback.
- Prefer evidence over optimism.

Start by summarizing the runbook, then walk me through the first three commands you want me to run.
```
