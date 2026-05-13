# Final Checklist

Use this near the end of the run.

## Code Scope

- Ticket diffs stayed within each ticket's allowlist
- No unintended descriptor or protocol redesign slipped in
- Fallback sign-of-life behavior was handled intentionally, not accidentally

## Runtime

- `--stats` opens the control interface
- Idle behavior matches the intended current phase:
  - fallback Num Lock sign-of-life present if still intentionally retained, or
  - no spontaneous typing if final idle behavior has been reached
- `--inject 0x04` produces one `a`
- Key release happens cleanly
- No stuck-key behavior remains

## Regression

- Reboot check performed
- Re-enable or redeploy check performed
- Stats still work after the regression checks

## Evidence

- `driverspike-debug-notes.md` updated with outcome notes
- Pass/fail recorded for each ticket
- Residual risk or follow-on bug clearly stated

## Success Statement Template

Use this only if it is true:

```text
The UMDF driver no longer depends on the old Caps Lock test behavior. The custom control interface opens successfully, stats are retrievable, and injected commands are translated into HID keyboard down/up reports without leaving a stuck key state.
```

## Honest Partial-Success Template

Use this if needed:

```text
The code path is substantially improved and the control/injection plumbing is now present, but the spike is not fully complete because <specific remaining issue>. The issue reproduces with <specific command or condition>, and the current evidence is recorded in driverspike-debug-notes.md.
```
