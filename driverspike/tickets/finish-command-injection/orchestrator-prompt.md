# Orchestrator Prompt - Finish Command Injection

You are orchestrating the final UMDF keyboard-spike push in this repository. Your job is to manage execution carefully, keep scope bounded to this ticket set, and produce verifiable progress at each step.

Use this prompt as your runbook. Do not expand scope beyond the tickets in:

- `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection`

## Objective

Finish the user-mode control path so the virtual keyboard driver:

- preserves a temporary Num Lock sign-of-life indicator during bring-up, then retires it only when appropriate
- accepts commands through the custom control interface
- translates those commands into HID keyboard reports on the read-completion path
- exposes trustworthy stats for operator validation

## Repository Context

Primary project roots:

- `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\driverspike.sln`
- `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver`
- `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TestEnumeratorAndClient`

Primary context files:

1. `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\driverspike-debug-notes.md`
2. `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\driverspike-common.h`
3. `C:\Users\artwh\Downloads\Star Commander MFD\driverspike\tickets\finish-command-injection\README.md`
4. Each ticket file in execution order

## Ticket Order

Execute in this order:

1. `T01-state-model-and-idle-behavior.md`
2. `T02-control-interface-open-path.md`
3. `T03-command-driven-report-emission.md`
4. `T04-client-stats-and-injection-validation.md`
5. `T05-hardening-and-evidence.md`

Do not skip ahead unless a prior ticket explicitly says to stop and report.

## Scope Guardrails

- UMDF only. No KMDF, VHF, or kernel-mode alternatives.
- No descriptor redesign unless a ticket explicitly authorizes it.
- No broad refactors.
- No renaming `TheSpikeyDriver` or `TestEnumeratorAndClient`.
- No changes outside the active ticket's allowlist without stopping and reporting.
- No install/redeploy retry loops. Two attempts maximum for the same root cause.

## Orchestrator Responsibilities

For each ticket:

1. Read the ticket and restate the goal in one paragraph.
2. Confirm the allowed files before editing starts.
3. Decide whether the ticket should be handled locally or by one or more subagents.
4. If you use subagents:
   - give each subagent one bounded responsibility
   - give each subagent explicit file ownership
   - do not assign overlapping write scopes
   - tell each subagent they are not alone in the codebase and must not revert others' edits
5. Review all returned changes yourself before declaring the ticket complete.
6. Run the required local verification that the ticket depends on.
7. Record pass/fail and the evidence.

## Subagent Policy

Use subagents only for bounded, non-overlapping tasks that materially reduce risk or speed up work.

Good delegation examples:

- one worker patches driver state fields in `Device.h` and `Device.c`
- one worker patches client-side output in `main.cpp`
- one explorer answers a narrow question about a trace path or open-path behavior

Bad delegation examples:

- two workers editing `manualqueue.c`
- a worker asked to “finish the whole feature”
- a worker assigned the immediate blocking task while you wait idle

When using subagents:

- provide the exact ticket file path
- provide the exact allowed file list
- require a final report with:
  - files touched
  - what changed
  - anything skipped
  - any stop condition encountered

## Bring-Up Discipline

Treat this as a stateful driver project, not a normal app change.

Important rules:

- Control-interface presence is not the same as control-interface usability.
- A successful `--stats` open is a gate before treating injection as done.
- A queued command is not done until the read path emits both down and up phases.
- Early-ticket success may still include an intentional Num Lock sign-of-life pattern.
- Final idle success means no spontaneous typing.
- Any sign of stuck-key behavior is a stop condition.

## Ticket Completion Checklist

A ticket is complete only when all of the following are true:

1. The diff stays inside the ticket's allowed files.
2. The code builds if the ticket requires a build check.
3. The required runtime validation for that ticket passes, or the failure is classified and reported.
4. The next ticket's assumptions now hold.

If a ticket fails:

- stop advancing
- classify the failure
- report the commands run, raw output, and the likely failing layer
- propose the smallest next action

## Stop Conditions

Stop immediately and report if any of these occur:

- a ticket appears to require touching files outside its allowlist
- the same install/redeploy failure happens twice
- the control interface still cannot be opened after the T02-scoped fixes
- injection produces stuck-key behavior
- runtime evidence contradicts a ticket's core assumption
- a subagent returns changes outside its assigned scope

## Reporting Format

After each ticket, report:

1. Ticket name
2. Status: pass, blocked, or partial
3. Files touched
4. Commands run
5. Evidence snippets
6. Risks or follow-up questions

At the end of the whole run, report:

1. Which tickets passed
2. Final runtime behavior
3. Remaining risks
4. Recommended next step if the spike is not fully complete

## Success Definition

The operation is successful only if:

- the old Caps Lock sign-of-life path is gone
- `--stats` opens the control interface and returns useful data
- `--inject <usage> [modifier]` produces the expected keyboard behavior
- the driver returns cleanly to idle after command completion
- the validation evidence is written down clearly
