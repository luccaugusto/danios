# danios — Handover / Progress Notes

**Last updated:** 2026-07-06 (F3 closed — SD storage, NVS settings persistence,
Settings→Tela/Unidades/Sobre, screen sleep, Portuguese UI + custom font, gears
icon from SD; all E2E-verified on hardware)
**Phase:** Foundation. **F1 (LVGL + touch), F2 (launcher) and F3 (storage +
settings) are DONE.** Next: **F4 (WiFi + time) → F5 (Bluetooth audio)** — start
from [`docs/HANDOVER-settings.md`](HANDOVER-settings.md).
Note: all user-facing UI is in **Portuguese** (launcher: Clima, Música,
Calculadora, Oráculo, Bichinho, Configurações). The default LVGL font is the
custom `montserrat_pt_14` (`src/assets/fonts/`, regen command in its README) —
UI strings may only use codepoints 0x20-0x7F, 0xA0-0xFF, 0x2022 (no em/en
dashes). SD-card art is staged in `sd/` (copy its contents to the card root). Per-app requirement
extracts (one file per app) live in
[`docs/superpowers/specs/apps/`](superpowers/specs/apps/).

Full product design lives in
[`docs/superpowers/specs/2026-06-03-esp32-gift-device-design.md`](superpowers/specs/2026-06-03-esp32-gift-device-design.md);
the milestone sequence is the roadmap
([`docs/superpowers/plans/2026-07-03-danios-roadmap.md`](superpowers/plans/2026-07-03-danios-roadmap.md)).
Environment setup is in the repo [`README.md`](../README.md).

---

## Where we are

F1 delivered the whole display+touch foundation, verified on hardware and
closed out with a whole-branch review (findings fixed or waived — see the
`fix: apply F1 whole-branch review findings` commit):

- **DisplayService** (`src/services/DisplayService.{h,cpp}`) — owns the LGFX
  instance and the LVGL 8.4 display driver. One static 240×30 draw buffer,
  synchronous swap-free flush (`LV_COLOR_16_SWAP 0`, cast to
  `lgfx::rgb565_t*`). Portrait 240×320 via `setRotation(7)`.
- **TouchService** (`src/services/TouchService.{h,cpp}`) — XPT2046 polled
  through LovyanGFX (`getTouchRaw` + `convertRawXY`; split deliberately to
  keep raw coords for the serial verification contract), debounced with
  `lib/press_debounce/` (native-tested), fed to LVGL as pointer indev.
- **`include/LGFX_ESP32_2432S024.hpp`** — the hardware truth: panel geometry,
  pins, and measured touch calibration. Renamed from `...S024C.hpp` (this is
  NOT the capacitive C variant).
- **`src/main.cpp`** — F1 smoke screen (tap-counter button). Placeholder that
  F2's launcher replaces.
- Host tests: `pio test -e native` runs `test/test_press_debounce`.

F2 then delivered the app framework on top of it, all live on hardware:
`src/core/App.h` (the pinned app interface), `Launcher` (grid of six icons —
5 stubs + Settings — lifecycle, back bar, badge + enabled plumbing),
`StatusBar`, the Settings shell (`src/apps/settings/SettingsApp.{h,cpp}`),
and `src/apps/app_catalog.h` — the **one place to edit an app's launcher name
or icon path** (apps' `title()`/`iconPath()` read from it).

### What the settings phase (F3–F5) starts from

Everything above is proven; SD, NVS, WiFi, time, and Bluetooth are untouched.
The three plans are written — execution order, gotchas, and definition of done
are in [`HANDOVER-settings.md`](HANDOVER-settings.md).

---

## Hard-won hardware knowledge (don't rediscover)

### Display root cause (2026-07-03)

The panel's controller is **not a genuine portrait ILI9341** — it's a
landscape-native 320×240 clone (ILI9342-class geometry):

- It fails the ILI9341 ID check — RDID4 (`0xD3`) reads all zeros where a real
  ILI9341 returns `0x93 0x41`. RDDID (`0x04`) returns `0x10 0x81 0xD9`.
- With the old (wrong) 240×320 config, even rotations only addressed 240 of
  320 physical columns ("~75% partial fills") and row addresses past 239
  wrapped modulo 240 (phantom lines, misplaced squares). Declaring
  `panel/memory = 320×240` fixed every rotation; portrait is rotation 7
  (MADCTL `MV|MX|MY`). `rgb_order = true` (RGB), `invert = false`.
- `offset_rotation` **must stay 0–3** — 4..7 corrupts the write window (per
  LovyanGFX maintainer). It stays 0.

### Touch plot twist (2026-07-04)

The month of CST820 I²C NACKs had a simple answer: **there is no CST820** —
this unit is the **resistive** 2432S024 variant. Touch is an XPT2046 **on the
shared display SPI bus** (CS 33, PENIRQ 36), configured in
`include/LGFX_ESP32_2432S024.hpp` with on-device measured calibration. The
min>max cal corners map raw → rotation-independent panel-native coords;
`convertRawXY` applies the active rotation, so a future `setRotation` change
needs no re-measurement. Full discovery trail: `.superpowers/sdd/progress.md`
(historical ledger); vendor reference: `docs/VENDOR-NOTES.md`; pin tables:
`docs/hardware.md`.

### Serial verification workflow

`pio device monitor` dies without a TTY — use
`python3 .superpowers/sdd/serial_capture.py --reset --seconds 10`. Start the
capture **before** tapping; prints are lost when nothing listens. Touch-downs
print `[touch] raw=(..) screen=(..)` — the four-corner calibration check
(ledger step 7, reused by the F5 fallback procedure) reads exactly that line.

---

## Hardware

- **Board:** ESP32-2432S024 ("Cheap Yellow Display", 2.4"), ESP32-WROOM-32,
  **resistive-touch variant**. Details + pin tables: `docs/hardware.md`.
- **Display:** landscape-native 320×240 ILI9341-class clone on HSPI; portrait
  UI via rotation 7. App-facing rules: `docs/DISPLAY.md`.
- **Touch:** XPT2046 resistive, shared display SPI — working, calibrated.
- **SD card:** separate bus (VSPI, CS 5) — untouched so far; F3 proves it.

## Toolchain

- **PlatformIO**, `platform = espressif32@7.0.1`, `framework = arduino`,
  `board = esp32dev`, C++17, `huge_app.csv` partitions.
- Flash over USB-C on `/dev/ttyUSB0` (CH340): `pio run -t upload`.
- Host tests: `pio test -e native` (the `cyd` env ignores test/ by design).

## Libraries (in use)

| Library | Source / version | Used for |
| --- | --- | --- |
| **LovyanGFX** | `lovyan03/LovyanGFX@^1.2.0` | Display + XPT2046 touch driver |
| **LVGL** | `lvgl/lvgl@8.4.0` (v8 API) | UI toolkit; config in `include/lv_conf.h` |
| Arduino-ESP32 core | bundled with `espressif32@7.0.1` | Serial, GPIO, SPI; later WiFi/BT/SD/NVS |

**Planned (per spec, not yet added):** ArduinoJson (weather/geo parsing),
ESP32-A2DP + arduino-audio-tools/libhelix (Bluetooth MP3 audio).

## Key files

```
platformio.ini                         PlatformIO project + deps (envs: cyd, native)
src/main.cpp                           boot flow + main loop (launcher wired)
src/core/                              App.h, Launcher, StatusBar (F2)
src/apps/app_catalog.h                 EDIT HERE: app launcher names + icon paths
src/apps/settings/                     Settings shell (sections arrive F3-F5)
src/services/DisplayService.{h,cpp}    LGFX + LVGL display driver glue
src/services/TouchService.{h,cpp}      XPT2046 -> LVGL pointer indev
lib/press_debounce/                    touch release debounce (native-tested)
include/LGFX_ESP32_2432S024.hpp        LovyanGFX board config — the hardware truth
include/lv_conf.h                      LVGL 8.4 config
docs/DISPLAY.md                        display essentials for app development — read first
docs/hardware.md                       board/pin reference
```

## Reference configs (authoritative for pins; treat orientation/touch flags with care)

- Sunton board def (pins): `rzeldent/platformio-espressif32-sunton` →
  `esp32-2432S024C.json`. ⚠️ Orientation flags assume portrait-native
  silicon; our clone is landscape-native (we need rotation 7). Pins correct.
- `rzeldent/esp32-smartdisplay` — battle-tested LVGL driver library for these boards.
- `hi631/ESP32-2432S024C` and `edmasini/esp32-2432S024-Capacitive` — LovyanGFX +
  touch examples for the **C (capacitive) variant** — NOT this unit's touch.
- LovyanGFX mirrored-panel rotation quirk: issues #711 and #600.
