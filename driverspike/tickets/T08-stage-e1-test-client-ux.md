# T08 — Stage E.1 (test client UX): Press Enter, 5s countdown, emit 'i'

**Stage:** E.1 user-facing acceptance
**Estimated effort:** small
**Touches:** `TestEnumeratorAndClient/main.cpp` (rewrite default mode; preserve `--probe-hid`)

## Goal

Replace the test client's default behavior (currently a transitional message from T06) with the spike's user-facing acceptance flow:

1. Enumerate the custom device interface (`GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL` from `Public.h`).
2. `CreateFile` it.
3. Print `Press Enter to send 'i' in 5 seconds...` and `getchar()`.
4. Print countdown `5...`, sleep 1s, `4...`, sleep 1s, ... `1...`, sleep 1s, `sending 'i'`.
5. `DeviceIoControl` with `IOCTL_THESPIKEYDRIVER_INJECT_KEY` and payload `{ HidUsage = 0x0C, Modifier = 0x00 }`.
6. Print result and exit.

`--probe-hid` mode (T06) must continue to work unchanged.

## Inputs

- Read first: `driverspike/phase2-plan.md` (Stage E "Test client UX" paragraph).
- Read first: `driverspike/phase2-references.md` (HID usage table — `'i'` is `0x0C`).
- Touch: `TestEnumeratorAndClient/main.cpp` only.

## Make ONLY these changes

1. In `TestEnumeratorAndClient/main.cpp`:
   - Add a function `RunInjectKeyDefault()` implementing steps 1–6 above.
   - Add a small enumeration helper that uses `CM_Get_Device_Interface_List_SizeW` + `CM_Get_Device_Interface_List` against `GUID_DEVINTERFACE_THESPIKEYDRIVER_CONTROL`. Pick the first present path (the spike installs at most one device).
   - The IOCTL payload is the 2-byte struct `THESPIKEYDRIVER_INJECT_KEY_INPUT` from `driverspike-common.h`.
   - The `CreateFile` access mask is `GENERIC_READ | GENERIC_WRITE`, share `0`, no template.
   - The countdown uses `Sleep(1000)` between numbers. Use `printf` + `fflush(stdout)` so the user sees each number as it prints.
   - Modify `main` so:
     - With no args → call `RunInjectKeyDefault()`.
     - With `--probe-hid` → existing T06 behavior unchanged.
     - With any other arg → print a short usage message and exit non-zero.
2. In `TestEnumeratorAndClient/TestEnumeratorAndClient.vcxproj`, ensure `Public.h` from the driver project is on the include path so the GUID can be referenced. If it is not currently, add the driver project's directory to `AdditionalIncludeDirectories` for both Debug|x64 and Release|x64 configurations. Do NOT add other include paths.
3. The GUID needs to be defined exactly once in the user-mode program. Either:
   - Include `<initguid.h>` before including `Public.h` in `main.cpp`, OR
   - Define the GUID inline in `main.cpp` with `DEFINE_GUID(...)` matching the values from `Public.h`.
   Use the `<initguid.h>` approach (cleaner, no risk of GUID drift).

## Do NOT

- Touch the driver, INF, common.h struct definitions, or build script.
- Add CLI args beyond `--probe-hid` (no `--list`, no `--verbose`, no flag soup).
- Print a banner, version string, or anything else cosmetic.
- Send anything other than `0x0C` (HID usage for 'i') in the default flow. The user will retarget keys later by editing the source — that's fine for the spike.
- Fall back to anything if enumeration fails — just print a clear error and exit non-zero.
- Reuse phase 1's `FindMatchingDevice` etc. for the custom-interface path (those are HID-stack helpers; the custom interface doesn't go through `HidD_*`).

## Acceptance criteria (executor self-verification)

- [ ] `TestEnumeratorAndClient.exe` (no args) waits for Enter, then counts down, then exits.
- [ ] `TestEnumeratorAndClient.exe --probe-hid` still does what T06 made it do.
- [ ] The IOCTL payload is exactly 2 bytes, populated as `{HidUsage=0x0C, Modifier=0x00}`.
- [ ] Solution builds clean.
- [ ] Diff touches only `main.cpp` (and `.vcxproj` if the include path needed updating).

## Stop and ask if

- The driver project's directory contains other headers that would conflict if added to the include path (unlikely — `Public.h` is the only public header).
- The custom-interface GUID enumeration returns zero paths after a fresh install (means T07's `WdfDeviceCreateDeviceInterface` call didn't take effect — flag for me, do not "fix" it from the test app).
- The IOCTL returns an error other than success — print it cleanly and exit; do not retry.
