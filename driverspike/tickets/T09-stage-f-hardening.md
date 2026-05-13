# T09 — Stage F: Hardening sweep + clean removal

**Stage:** F (last ticket — no new features, just durability checks)
**Estimated effort:** small
**Touches:** none (this is primarily a verification ticket)

## Goal

Confirm Phase 2 acceptance reproduces under all the hostile conditions defined in the plan: clean reboot, `devcon disable/enable`, and full driver removal followed by reinstall. Capture the evidence in a short summary written to `driverspike/phase2-spike-evidence.md`.

This ticket has no significant code work. If a regression is discovered, the executor STOPS and asks rather than fixing — fixes are new tickets.

## Inputs

- Read first: `driverspike/phase2-plan.md` (Definition of Done, Stage F).
- Read first: prior ticket evidence (T05, T08) for reference behavior.
- Touch: create `driverspike/phase2-spike-evidence.md` only.

## Make ONLY these changes

1. Create `driverspike/phase2-spike-evidence.md` with sections:
   - Build evidence (paste of `Build-VS2022.cmd` output tail).
   - Stage C evidence (HIDView screenshot path or text dump, `HidP_GetCaps` output, `devcon stack` output).
   - Stage D evidence (one-paragraph description of timer-driven 'A' appearing in Notepad with no test app running, plus WPP trace excerpt).
   - Stage E evidence (one-paragraph description of `TestEnumeratorAndClient.exe` press-Enter-countdown-emit-'i' working in Notepad).
   - Reproducibility evidence: results of (a) clean reboot then re-test, (b) `devcon disable` + `devcon enable` then re-test, (c) full uninstall via `Redeploy-TheSpikeyDriver.ps1` reinstall flow then re-test.
   - Removal evidence: `pnputil /enum-drivers` output before and after final removal showing the OEM entry gone.
2. The user runs the actual tests (see manual). The executor's job in this ticket is just to template the file with `TODO:` placeholders for each section, so the user can fill in evidence as they run tests.

## Do NOT

- Modify any source code, INF, or build script.
- Attempt to "fix" anything you observe during evidence gathering.
- Re-run failed tests more than once before stopping and asking.
- Delete `phase2-plan.md` or `open-questions-phase2.md` even after Phase 2 is "done" — they remain as historical record.

## Acceptance criteria (executor self-verification)

- [ ] `phase2-spike-evidence.md` exists with all sections present (placeholders OK — user fills in).
- [ ] No source files were modified.

## Stop and ask if

- The user-provided test results show any acceptance criterion failing.
- The driver fails to remove cleanly via the Redeploy script's removal logic.
