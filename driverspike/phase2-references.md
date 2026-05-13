# Phase 2 References

This document locks down the core facts and reference data for the Phase 2 Keyboard Injector spike.

## 1. Boot keyboard report descriptor (candidate bytes)

This descriptor follows the HID Usage Tables 1.4 §B.1 Boot Keyboard specification. It defines an 8-byte input report and a 1-byte output report.

**Verification:**
- **InputReportByteLength:** 64 bits = 8 bytes (1 modifier + 1 reserved + 6 keycodes).
- **OutputReportByteLength:** 8 bits = 1 byte (5 LED bits + 3 padding bits).
- **Report ID:** None (No `0x85` opcodes).
- **`HID_DESCRIPTOR.wReportLength`** must equal `sizeof(report descriptor)` exactly. Currently enforced via `sizeof(G_DefaultReportDescriptor)` in `TheSpikeyDriver/hid.c` — do not hand-edit to a literal.
- **Logical Max 101 (`0x65`)** caps the keycode array to standard keys (A–Z, 0–9, F1–F12, navigation). F13–F24, media keys, and keypad operators above `0x65` would require raising this — out of scope for the spike.

```c
// Boot Keyboard Report Descriptor (Const+Var+Abs flags for padding per modern convention)
0x05, 0x01,                    // Usage Page (Generic Desktop)
0x09, 0x06,                    // Usage (Keyboard)
0xA1, 0x01,                    // Collection (Application)
0x05, 0x07,                    //   Usage Page (Keyboard/Keypad)
0x19, 0xE0,                    //   Usage Minimum (224) - Left Ctrl
0x29, 0xE7,                    //   Usage Maximum (231) - Right GUI
0x15, 0x00,                    //   Logical Minimum (0)
0x25, 0x01,                    //   Logical Maximum (1)
0x75, 0x01,                    //   Report Size (1)
0x95, 0x08,                    //   Report Count (8)
0x81, 0x02,                    //   Input (Data, Variable, Absolute) ; Modifier byte
0x95, 0x01,                    //   Report Count (1)
0x75, 0x08,                    //   Report Size (8)
0x81, 0x03,                    //   Input (Constant, Variable, Absolute) ; Reserved byte
0x95, 0x05,                    //   Report Count (5)
0x75, 0x01,                    //   Report Size (1)
0x05, 0x08,                    //   Usage Page (LEDs)
0x19, 0x01,                    //   Usage Minimum (1) - Num Lock
0x29, 0x05,                    //   Usage Maximum (5) - Kana
0x91, 0x02,                    //   Output (Data, Variable, Absolute) ; LED report
0x95, 0x01,                    //   Report Count (1)
0x75, 0x03,                    //   Report Size (3)
0x91, 0x03,                    //   Output (Constant, Variable, Absolute) ; LED report padding
0x95, 0x06,                    //   Report Count (6)
0x75, 0x08,                    //   Report Size (8)
0x15, 0x00,                    //   Logical Minimum (0)
0x25, 0x65,                    //   Logical Maximum (101)
0x05, 0x07,                    //   Usage Page (Keyboard/Keypad)
0x19, 0x00,                    //   Usage Minimum (0)
0x29, 0x65,                    //   Usage Maximum (101)
0x81, 0x00,                    //   Input (Data, Array) ; Key array (6 bytes)
0xC0                           // End Collection
```

## 1a. Wire-format input report layout (the contract on every keystroke)

Every 8-byte input report sent up the keyboard stack has this exact byte layout:

| Byte | Field | Notes |
|---|---|---|
| 0 | Modifier bitmap | LCtrl=0x01, LShift=0x02, LAlt=0x04, LGUI=0x08, RCtrl=0x10, RShift=0x20, RAlt=0x40, RGUI=0x80 |
| 1 | Reserved | Must be 0 (constant per descriptor) |
| 2 | Keycode array slot 0 | HID usage code from Section 2; 0 = no key |
| 3 | Keycode array slot 1 | 0 = no key |
| 4 | Keycode array slot 2 | 0 = no key |
| 5 | Keycode array slot 3 | 0 = no key |
| 6 | Keycode array slot 4 | 0 = no key |
| 7 | Keycode array slot 5 | 0 = no key |

Rules:
- Unused slots are `0`. Order within the array does not matter.
- Maximum 6 simultaneous keys (boot keyboard limit; NKRO would require a different descriptor).
- Each report represents the *current state* of the keyboard, not events. `kbdhid` infers press/release events by diffing consecutive reports.

## 2. Keyboard/keypad HID usage table — minimal subset

These are HID Usage Codes (Usage Page 0x07), not Virtual-Key (VK) codes or ASCII values.

| Key | HID Usage Code (Hex) |
|---|---|
| 'a' .. 'z' | `0x04` .. `0x1D` |
| '1' .. '9' | `0x1E` .. `0x26` |
| '0' | `0x27` |
| Enter | `0x28` |
| Escape | `0x29` |
| Backspace | `0x2A` |
| Tab | `0x2B` |
| Space | `0x2C` |
| **'i' (Target)** | **`0x0C`** |

**Modifiers (Byte 0):**
- Bit 0 (`0x01`): Left Control
- Bit 1 (`0x02`): **Left Shift**
- Bit 2 (`0x04`): Left Alt
- Bit 3 (`0x08`): Left GUI
- Bit 4 (`0x10`): Right Control
- Bit 5 (`0x20`): Right Shift
- Bit 6 (`0x40`): Right Alt
- Bit 7 (`0x80`): Right GUI

**Worked examples (the #1 mistake is treating these like ASCII):**
- `'a'` → input report `{ Modifier=0x00, Reserved=0x00, Keycodes={0x04, 0, 0, 0, 0, 0} }`.
- `'A'` → input report `{ Modifier=0x02, Reserved=0x00, Keycodes={0x04, 0, 0, 0, 0, 0} }`. (Same usage code — uppercase comes from the Shift modifier, not a different key.)
- `'i'` (the spike target) → `{ Modifier=0x00, Reserved=0x00, Keycodes={0x0C, 0, 0, 0, 0, 0} }`.
- All-zeros (release / idle baseline) → `{ Modifier=0x00, Reserved=0x00, Keycodes={0, 0, 0, 0, 0, 0} }`.

ASCII `'a'` is `0x61`, which is **not** a valid keycode. ASCII `'A'` is `0x41`, also not a valid keycode. Sending ASCII into the keycode slots will either type nothing or type an unrelated key (`0x41` = Caps Lock).

## 3. Expected device stacks (post-Stage-C)

**Critical understanding:** When `hidclass.sys` parses our keyboard descriptor, it spawns a **child PDO** for the keyboard top-level collection. `kbdhid` attaches to **that child**, not to our root device. So you have to query *two* stacks to verify Stage C — and `kbdhid` will **never** appear in the parent's stack output.

### 3a. Parent stack (our UMDF minidriver)

`devcon stack root\TheSpikeyDriver` shows the UMDF stack only. **`kbdhid` is intentionally absent here.**

```text
ROOT\THESPIKEYDRIVER\0000
    Name: TheSpikeyDriver virtual HID device
    Setup Class: {745a17a0-74d3-11d0-b6fe-00a0c90f57da} HIDClass
    Controlling service:
        WUDFRd            (or mshidumdf, depending on which AddService binds primary)
    Lower filters:
        mshidumdf         (or WUDFRd)
1 matching device(s) found.
```

If this stack looks broken (e.g., no service, ConfigManagerErrorCode != 0), the UMDF driver itself isn't running — fix that before worrying about kbdhid.

### 3b. Child PDO stack (the keyboard kbdhid binds to)

`hidclass.sys` spawns a child with a hardware ID like `HID\VID_DEED&PID_FEED&Col01\…`. Query it with a wildcard:

`devcon stack "HID\VID_DEED&PID_FEED*"`

```text
HID\VID_DEED&PID_FEED&Col01\...
    Name: HID Keyboard Device
    Setup Class: {4d36e96b-e325-11ce-bfc1-08002be10318} Keyboard
    Controlling service:
        kbdhid
    Upper filters:
        kbdclass
1 matching device(s) found.
```

**This is the stack that proves kbdhid attachment.** Note the role split: `kbdhid` is the controlling service (the HID-to-keyboard mapper), `kbdclass` is layered above as the abstract class driver.

### 3c. Authoritative go/no-go signal

Stack output formatting can vary across Windows builds. The single most reliable kbdhid-attachment check is service binding via WMI:

```powershell
Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.Service -eq 'kbdhid' -and $_.PNPDeviceID -like '*VID_DEED*' } |
  Select-Object Name, PNPDeviceID, Service, Status
```

Substitute the actual VID assigned in the driver. If at least one row comes back with `Status = OK`, kbdhid has attached and Stage C is functionally green — regardless of how `devcon stack` formats its output.

## 4. Expected child PDO compatible ID

The `hidclass.sys` driver, upon parsing a Keyboard Top-Level Collection (Usage Page 1, Usage 6), spawns a child Physical Device Object (PDO).

- **Compatible ID:** `HID_DEVICE_SYSTEM_KEYBOARD`
- **Matching INF:** `input.inf`

This compatible ID triggers the loading of `kbdhid.sys` (HID-to-Keyboard mapper) and `kbdclass.sys` (Keyboard class driver). This mechanism allows our minidriver to integrate into the standard Windows keyboard stack without custom INF entries for the keyboard functionality.

## 5. Stage E.0 result log

Filled in during T06 — exact `GetLastError()` value from `CreateFile` against the keyboard HID interface path after Stage C.
