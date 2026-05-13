# T05 - Hardening And Evidence

## Goal

Prove the finished command path is stable enough for the spike by exercising the important lifecycle checks and recording the results.

## Files In Scope

- `driverspike-debug-notes.md`
- `tickets\finish-command-injection\README.md`

This ticket is intentionally light on source changes. Most work is validation and evidence capture.

Stop and ask before touching any other file.

## Required Validation Matrix

Run these checks and record pass/fail evidence:

1. Idle behavior after install
   - if the fallback sign-of-life path is still intentionally present at this point, record that clearly
   - if the fallback sign-of-life path has been removed by this point, confirm no spontaneous typing remains

2. Control path open
   - `TestEnumeratorAndClient.exe --stats`

3. Single injection
   - `TestEnumeratorAndClient.exe --inject 0x04`
   - expected one `a`

4. Modified injection
   - `TestEnumeratorAndClient.exe --inject 0x04 0x02`
   - expected shifted `a` behavior if focus target allows it

5. Repeat cycle behavior
   - use an `EmitCount > 1` path if available
   - expected repeated down/up cycles without stuck key state

6. Reboot regression
   - repeat `--stats`
   - repeat single injection

7. Device disable/enable or redeploy regression
   - repeat `--stats`
   - repeat single injection

## Make ONLY These Changes

1. Add a dated evidence section to `driverspike-debug-notes.md` summarizing:
   - commands run
   - observed results
   - open questions or residual risks
   - whether the fallback Num Lock sign-of-life behavior was still present or had been retired

2. Update this folder's `README.md` only if the observed results require a short note about current known limits.

## Do NOT

- Do not widen code scope just because a validation step reveals a possible cleanup.
- Do not rewrite the notes file.
- If validation reveals a real bug, stop and report it rather than quietly fixing it under this ticket.

## Acceptance Criteria

- Evidence exists for idle, stats, inject, reboot, and re-enable/redeploy checks.
- Any residual risk is documented plainly.
- The ticket set ends with a truthful state summary, not an optimistic one.

## Report Back With

- Exact files touched.
- Validation steps run.
- Pass/fail summary.
- Any unresolved issue that should become a follow-on ticket.
