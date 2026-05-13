# T01 — Stage A: Reference research

**Stage:** A (no code, doc only)
**Estimated effort:** small
**Touches:** new file `driverspike/phase2-references.md`

## Goal

Produce a single reference document (`driverspike/phase2-references.md`) that locks down the four facts every later stage depends on:

1. The exact byte sequence of the HID boot keyboard report descriptor we will use in Stage C.
2. The HID usage codes (not VK / not ASCII) for the keys we'll inject in Stages D and E.
3. The expected post-install device stack (`devcon stack` output).
4. The expected child-PDO compatible ID that triggers `kbdhid` attachment.

## Inputs

- Read first: `driverspike/phase2-plan.md` (especially Critical Invariants + Stage A).
- Read first: `driverspike/open-questions-phase2.md`.
- Source: HID Usage Tables 1.4 §B.1 and §10 (boot keyboard descriptor, keyboard/keypad usage page).

## Required content of `phase2-references.md`

The file must include the following sections, in this order:

1. **Boot keyboard report descriptor (candidate bytes).** Hex byte array with one comment per HID item line explaining what the byte means (Usage Page, Collection, Logical Min/Max, Report Size, Report Count, Input/Output, etc.). No `0x85` (Report ID) opcode anywhere. Computed `InputReportByteLength` must equal exactly 8. Output report (LEDs) must be exactly 1 byte (5 bits LED + 3 bits padding).
2. **Keyboard/keypad HID usage table — minimal subset for the spike.** At minimum: `'a'..'z'` (`0x04`..`0x1D`), digits `'1'..'0'` (`0x1E`..`0x27`), Enter `0x28`, Space `0x2C`. Explicitly mark `'i'` = `0x0C` (the spike's required keystroke). Note that left-shift modifier bit is `0x02` in modifier byte 0.
3. **Expected device stack (post-Stage-C).** A literal `devcon stack root\TheSpikeyDriver` expected-output block showing `kbdhid → mshidumdf → WUDFRd` (the actual format `devcon` prints — see Microsoft docs for `devcon stack`).
4. **Expected child PDO compatible ID.** Statement that `hidclass.sys` spawns a child PDO carrying compatible ID `HID_DEVICE_SYSTEM_KEYBOARD`, which `input.inf` matches to load `kbdhid` + `kbdclass`. This is the mechanism for kbdhid attachment — descriptor-driven, no INF changes needed in our minidriver.
5. **Stage E.0 result log.** Empty placeholder section: "Filled in during T06 — exact `GetLastError()` value from `CreateFile` against the keyboard HID interface path after Stage C."

## Make ONLY these changes

- Create `driverspike/phase2-references.md` with the content above.

## Do NOT

- Modify any source code, INF, or PowerShell script.
- Modify `phase2-plan.md`, `open-questions-phase2.md`, or any phase 1 document.
- Add a "phase2-references" folder — a single flat `.md` file is what's wanted.

## Acceptance criteria (the executor verifies these before reporting done)

- [ ] File `driverspike/phase2-references.md` exists.
- [ ] Section 1 contains a hex byte array with no `0x85` byte. Visually compute the input report size from the descriptor: `Report Size × Report Count` for each Input item, summed, must equal exactly 64 bits = 8 bytes. (Modifier: 1×8=8 bits. Reserved: 8×1=8 bits. Keycode array: 8×6=48 bits. Total 64.)
- [ ] Section 2 explicitly states `'i' = 0x0C` and `Left Shift = modifier byte 0x02`.
- [ ] Section 3 references the documented `devcon stack` output format.
- [ ] Section 4 names `HID_DEVICE_SYSTEM_KEYBOARD` and `input.inf` by name.
- [ ] Section 5 placeholder exists.

## Stop and ask if

- The descriptor byte sequence you computed produces an `InputReportByteLength` other than 8.
- You cannot find an authoritative source for the boot keyboard descriptor (do not invent bytes from memory).
- Anything in `phase2-plan.md` contradicts what this ticket asks for.
