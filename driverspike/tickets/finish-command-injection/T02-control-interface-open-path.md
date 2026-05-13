# T02 - Control Interface Open Path

## Goal

Make the custom control interface reliably openable from user mode so `--stats` and `--inject` can reach the driver.

## Files In Scope

- `TheSpikeyDriver\Device.c`
- `TheSpikeyDriver\hid.c`
- `TheSpikeyDriver\Queue.c`
- `TheSpikeyDriver\Driver.c`
- `TheSpikeyDriver\Trace.h`
- `TestEnumeratorAndClient\main.cpp`

Stop and ask before touching any other file.

## Current Problem

Runtime evidence showed:

- the custom interface path enumerates
- `CreateFile` on that path fails with Win32 error 31

That means the interface publishing step alone is not enough. We need to validate the open path and make it observable in trace.

## Make ONLY These Changes

1. Audit and improve the control-interface open path in the driver.
   Focus areas:
   - device interface create path
   - file-create callback behavior
   - request routing for the control path
   - trace visibility during interface opens

2. Add tracing that makes these questions answerable from logs:
   - did user mode try to open the interface
   - did `EvtDeviceFileCreate` run
   - was the request completed successfully
   - if not, what status was returned

3. In the client, improve open-path diagnostics only if needed.
   For example:
   - print which access mode succeeded or failed
   - print the specific path being opened
   - keep the behavior non-destructive

4. Preserve the existing interface GUID and the existing `--stats` and `--inject` entry points.

## Do NOT

- Do not redesign the public protocol.
- Do not implement the report emission logic in this ticket.
- Do not change the INF unless runtime evidence proves the open failure is caused by install-time policy and you cannot solve it in code.
  If that happens, stop and ask first.

## Acceptance Criteria

- `TestEnumeratorAndClient.exe --stats` can open the control interface successfully.
- The driver trace can show a successful open path.
- If the interface still fails to open, the logs now make the specific failing step obvious.

## Suggested Validation

- Build and deploy.
- Run `TestEnumeratorAndClient.exe --stats`.
- If it fails, capture the trace and the exact error.
- If it succeeds, preserve the trace proving the file-create/open path was hit.

## Report Back With

- Exact files touched.
- Whether the root cause was in open handling, routing, or only in diagnostics.
- Whether `--stats` fully works by the end of this ticket.
