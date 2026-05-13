# Operator Runbook - Finish Command Injection

This is the practical companion to the ticket set. Use it when you want the orchestrator to do the code work while you keep the runtime checks deliberate and safe.

## What You Have

- A ticket packet in this folder
- An orchestrator runbook
- A bounded set of files and validations

## What You Still Need To Do

You are still the human in the loop for:

- reviewing diffs after each ticket
- deciding whether to allow runtime deploy/test steps
- watching for unexpected keyboard behavior
- stopping the run if the machine starts behaving strangely

## Recommended Session Setup

Open these before you start:

1. A normal PowerShell in:
   `C:\Users\artwh\Downloads\Star Commander MFD\driverspike`
2. An elevated PowerShell in the same folder for deploy steps.
3. Your editor on:
   - `driverspike-debug-notes.md`
   - this folder's `README.md`
   - this folder's current ticket
4. A simple text target like Notepad for injection tests.

## Safe Working Rhythm

For each ticket:

1. Let the orchestrator read the ticket and make the code changes.
2. Review the diff yourself before any deploy step.
3. Build.
4. If the ticket requires runtime validation, deploy and test once.
5. If the same deploy failure happens twice with the same root cause, stop.
6. Have the orchestrator record pass/fail before moving to the next ticket.

## Commands You Will Probably Use

Build:

```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\Build-VS2022.cmd"
```

Redeploy:

```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1"
```

Stats:

```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\x64\Debug\TestEnumeratorAndClient.exe" --stats
```

Inject one key:

```powershell
& "C:\Users\artwh\Downloads\Star Commander MFD\driverspike\x64\Debug\TestEnumeratorAndClient.exe" --inject 0x04
```

## Runtime Expectations By Milestone

After T01:

- the sign-of-life behavior should still exist
- it should use Num Lock rather than Caps Lock

After T02:

- `--stats` should open the control interface successfully, or the failure should be precisely classified

After T03:

- `--inject` should produce a down/up sequence through the HID read path

After T04:

- stats and client output should clearly describe the live command state

After T05:

- you should have enough evidence to say whether the spike is stable or still has a follow-on bug
- you should also know whether the fallback Num Lock sign-of-life behavior has been retired yet or intentionally kept during further bring-up

## Stop Immediately If

- the machine starts typing on its own after a ticket claims idle behavior is fixed
- a key appears stuck down
- the behavior changes back to Caps Lock flashing after T01 claims the switch was made
- the same deploy failure happens twice
- the orchestrator proposes edits outside the active ticket's file list
- a worker reports that it needed to broaden scope to finish a ticket

## Suggested Review Questions

When reviewing a ticket diff, ask:

1. Did it stay inside the ticket's file allowlist?
2. Did it solve only the problem the ticket described?
3. Did it quietly change protocol or behavior outside scope?
4. Would I know how to test this from the ticket alone?

## End Of Run

At the end, you want three things:

1. The code changes
2. A pass/fail record by ticket
3. A truthful statement of current runtime behavior

If the final answer is "mostly works except for X," that is still a good outcome as long as X is clearly documented.
