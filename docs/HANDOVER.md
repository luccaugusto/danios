# danios — Handover / Progress Notes

**Last updated:** 2026-07-03 (display bring-up ✅ SOLVED)
**Phase:** Milestone 1 — display bring-up (first hardware step of the build)

Full product design lives in
[`docs/superpowers/specs/2026-06-03-esp32-gift-device-design.md`](superpowers/specs/2026-06-03-esp32-gift-device-design.md).
Environment setup is in the repo [`README.md`](../README.md).

---

## What we're doing right now

Getting the screen to light up correctly is milestone 1 of the bring-up sequence
(`display → touch → SD → WiFi/weather → Bluetooth/audio`). Nothing else is built
until the display is solid, because everything (LVGL, all four apps) renders on it.

The current sketch (`src/main.cpp`) is a **diagnostic**, not product code: it fills
the screen, draws a white border (to check full-panel coverage) and red/blue corner
markers (to check orientation), and prints the resolution + a serial heartbeat.

### Status of the display config — SOLVED (2026-07-03)

| Setting | Value | Status |
| --- | --- | --- |
| Controller | **landscape-native 320×240 clone** (ILI9342-class geometry), driven by `Panel_ILI9341` with overridden dims | ✅ confirmed (see below) |
| Pins (SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, RST none, BL 27) | — | ✅ confirmed (matches Sunton board def + TFT_eSPI reference) |
| `panel_width`/`memory_width` × `panel_height`/`memory_height` | **320 × 240** (not 240×320!) | ✅ confirmed by eye |
| `rgb_order` | `true` (RGB) | ✅ confirmed by eye (BGR rendered teal as yellow) |
| `invert` | `false` | ✅ confirmed |
| Rotation for portrait, USB-C down | **`setRotation(7)`** (MADCTL `MV\|MX\|MY` — swap + both mirrors) | ✅ confirmed by eye: full fill, text upright, red TL / blue BR |

### Root cause of the old "rotation vs. fill" mystery

The panel's controller is **not a genuine portrait ILI9341**:

- It fails the ILI9341 ID check — RDID4 (`0xD3`) reads all zeros where a real
  ILI9341 returns `0x93 0x41`. RDDID (`0x04`) returns `0x10 0x81 0xD9`.
  MADCTL/readback (`0x0B`) works and matches whatever LovyanGFX writes.
- Photos of the rotation sweep proved its **column axis runs along the 320px
  long side**: non-swapped (even) rotations drew text along the long axis,
  which is impossible on portrait-native silicon.

With the old (wrong) 240×320 config, every symptom followed mechanically:
even rotations only addressed 240 of the 320 physical columns (the "~75%
partial fills" — 240/320 = 0.75), and row addresses past 239 **wrap modulo
240** (that produced the phantom white line / misplaced corner squares).
There was never a LovyanGFX bug and never a mirrored-panel exotica — just
transposed panel geometry. Declaring 320×240 makes every rotation address
cleanly; portrait is simply an odd (axis-swapped) rotation.

Gotcha that still stands (in `include/LGFX_ESP32_2432S024C.hpp`):
`offset_rotation` **must be 0–3** — 4..7 corrupts the write window (per
LovyanGFX maintainer). Not needed anymore; it stays 0.

**Display milestone is done** — `src/main.cpp` shows the full-screen teal +
corner markers + centered "hello danios" in portrait, USB-C down.
**Next up: F1 close-out.** The final whole-branch review + stale-doc/dead-code
cleanup is specced in
[`docs/superpowers/plans/2026-07-04-f1-final-review-and-cleanup.md`](superpowers/plans/2026-07-04-f1-final-review-and-cleanup.md)
— start there next session.

**Touch milestone — SOLVED (2026-07-04).** The month of CST820 NACKs had a
plot twist: this unit is the **resistive** 2432S024 variant — there is no
CST820 on the board. Touch is an **XPT2046 on the shared display SPI bus**
(CS 33, PENIRQ 36), now configured inside `include/LGFX_ESP32_2432S024C.hpp`
with on-device measured calibration; `TouchService` polls it via LovyanGFX
(`getTouchRaw`/`convertRawXY`) and feeds the LVGL pointer indev. Tap counter
verified on hardware. Full discovery trail: `.superpowers/sdd/progress.md`;
vendor reference material: `docs/VENDOR-NOTES.md`. `docs/hardware.md` has the
corrected pin table.

---

## Hardware

- **Board:** ESP32-2432S024 ("Cheap Yellow Display", 2.4"), ESP32-WROOM-32.
  **RESISTIVE-touch variant** (discovered 2026-07-04 — docs named it S024C
  but there's no capacitive controller on this unit).
- **Display:** ILI9341-class landscape-native clone, 320×240, on HSPI.
  **SD card** is on a separate bus (VSPI, CS 5).
- **Touch:** XPT2046 resistive, shared display SPI (CS 33, PENIRQ 36) —
  working; config + calibration in `include/LGFX_ESP32_2432S024C.hpp`.

## Toolchain

- **PlatformIO Core 6.1.19**, `platform = espressif32@7.0.1`, `framework = arduino`
  (arduino-esp32 3.x / ESP-IDF 5.x), `board = esp32dev`.
- Flashing over USB-C on `/dev/ttyUSB0` (CH340). Build + flash from repo root: `pio run -t upload`.
- Config in [`platformio.ini`](../platformio.ini).

## Libraries

| Library | Source / version | Used for |
| --- | --- | --- |
| **LovyanGFX** | `lovyan03/LovyanGFX@^1.2.0` (PlatformIO reg) | Display driver (SPI ILI9341) + graphics. Bound to LVGL later. |
| Arduino-ESP32 core | bundled with `espressif32@7.0.1` | Serial, GPIO, SPI, and later WiFi/BT/SD/NVS. |

**Planned (per the design spec, not yet added):** LVGL (UI), ArduinoJson (weather/geo
parsing), ESP32-A2DP + arduino-audio-tools/libhelix (Bluetooth MP3 audio).

## Key files

```
platformio.ini                          PlatformIO project + deps
src/main.cpp                            diagnostic bring-up sketch (throwaway)
include/LGFX_ESP32_2432S024C.hpp        LovyanGFX board config (the real deliverable so far)
docs/DISPLAY.md                         display essentials for app development — read this first
```

## Reference configs (authoritative for pins; treat orientation flags with care)

- Sunton board def (pins): `rzeldent/platformio-espressif32-sunton` → `esp32-2432S024C.json`
  — states `DISPLAY_SWAP_XY=false`, `DISPLAY_MIRROR_X=true`, `DISPLAY_MIRROR_Y=false`.
  ⚠️ Those flags assume a portrait-native 240×320 controller; our unit's clone is
  landscape-native 320×240, so the flags don't transfer literally (we needed
  swap+both-mirrors, i.e. LovyanGFX rotation 7). Pin assignments were all correct.
- `rzeldent/esp32-smartdisplay` — battle-tested LVGL driver library for these boards.
- `hi631/ESP32-2432S024C` and `edmasini/esp32-2432S024-Capacitive` — LovyanGFX + touch examples.
- LovyanGFX mirrored-panel rotation quirk: issues #711 and #600.
