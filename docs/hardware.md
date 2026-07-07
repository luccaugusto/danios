# danios hardware reference

**Last updated:** 2026-07-04

## Board on hand (flashed, verified): ESP32-2432S024 "CYD" 2.4" — RESISTIVE variant

The unit is the **resistive** 2432S024 variant — XPT2046 on the shared display
SPI bus (CS 33, PENIRQ 36), **no CST820 / no touch I²C bus at all**. Proven by
on-device diagnostics 2026-07-04 (I²C dead on every address, PENIRQ fires on
press, XPT2046 returns coordinates on the display bus pins). The working
display + touch config with measured calibration lives in
`include/LGFX_ESP32_2432S024.hpp` — don't re-derive it. Discovery trail:
`docs/VENDOR-NOTES.md` and `.superpowers/sdd/progress.md`.

Module: ESP-WROOM-32 (WiFi + **Bluetooth Classic**, A2DP-capable ✓, dual core,
4 MB flash), CH340 USB-serial → `/dev/ttyUSB0`. Plus display, touch, SD slot,
battery circuit (IP5603).

> Sources [A]/[B] below document the **C (capacitive) variant**; they're right
> about most pins but wrong about touch and some display details for this unit.
> Facts marked *verified* were measured on this board.
>
> - [A] https://github.com/edmasini/esp32-2432S024-Capacitive (`hal/esp32/app_hal.h`, `LGFX_ESP32_2432024C.hpp`, `CST820.h`)
> - [B] https://github.com/rzeldent/platformio-espressif32-sunton (`esp32-2432S024C.json`)

### Display (verified) — landscape-native 320×240 clone, HSPI

| Signal | GPIO |
| --- | --- |
| SCLK | 14 |
| MOSI | 13 |
| MISO | 12 |
| CS | 15 |
| DC | 2 |
| RST | none (-1) |
| Backlight | 27 (PWM channel 7 @ 44.1 kHz) |

- SPI: HSPI (SPI2_HOST), mode 0, 40 MHz write / 16 MHz read (verified).
- `invert = false`; **RGB** color order (`rgb_order = true`) — verified by eye
  (the C-variant sources say BGR; this unit differs).
- The controller is **not** a genuine portrait ILI9341: it's landscape-native
  320×240 silicon (fails the RDID4 check — reads zeros; RDDID `0x04` =
  `0x10 0x81 0xD9`). Config declares `panel/memory = 320×240`; the portrait UI
  is rotation 7. Full story + app-facing rules: `docs/DISPLAY.md`.

### Touch (verified) — XPT2046 resistive, shared display SPI

| Signal | GPIO |
| --- | --- |
| SCLK / MOSI / MISO | 14 / 13 / 12 (same as display) |
| CS | 33 |
| PENIRQ | 36 (low while pressed; 10k pull-up on board) |

- LovyanGFX `Touch_XPT2046`, `spi_host = SPI2_HOST`, `bus_shared = true`, 1 MHz.
- Calibration (raw 12-bit, portrait USB-down): an 8-value corner array captured
  with `calibrateTouch` at rotation 7, applied in `DisplayService::begin()` via
  `setTouchCalibrate`. Replaced the earlier hand-measured x_min/x_max/y_min/y_max
  box constants, which mis-scaled the horizontal axis (right-side taps drifted
  ~one key right). To re-calibrate, re-run `calibrateTouch` and paste the array.
- Resistive = needs firm/pointed presses; no multitouch, no gestures. The first
  contact sample reads offset (down-and-right) until pressure settles, so
  `PressDebounce` requires 2 consecutive samples before reporting a press
  (`press_settle=2`) — otherwise one tap can register its target plus a neighbour.

### microSD — separate bus (VSPI defaults; per [A]/[B], not yet exercised — F3 proves it)

| Signal | GPIO |
| --- | --- |
| SCK | 18 |
| MISO | 19 |
| MOSI | 23 |
| CS | 5 |

Arduino: `SD.begin(5)`. Display+touch (HSPI) and SD (VSPI) are separate buses.

### Other peripherals (per [A]/[B], unverified on this unit)

- RGB LED: R=4, G=16, B=17 (active-low, polarity not documented upstream).
- Photoresistor (CDS): GPIO 34.
- Speaker amp (8002-class mono): GPIO 26.
- **No battery-voltage ADC.** The IP5603 handles charge/boost only; battery %
  is NOT measurable on this board (spec §3.3/§6.4 can't show a real percentage).

### Touch — CST820, I²C (⚠️ C-variant only — NOT this unit)

Kept for reference in case a C-variant board ever shows up.

| Signal | GPIO |
| --- | --- |
| SDA | 33 |
| SCL | 32 |
| RST | 25 |
| INT | 21 ⚠ (some units use 22 — **poll, don't rely on INT**) |

- Address **0x15**, 400 kHz.
- Chip sleeps: pulse RST low→high and wait ~300 ms before first I²C access.
- Driver options: [A] ships its own small polling CST820 driver (known-good,
  handles all 4 rotations); CST820 is register-compatible with CST816S basic
  reads (esp_lcd + ESPHome evidence), so LovyanGFX `Touch_CST816S` at 0x15
  *likely* works but is unverified.

## Also on hand: bare ESP32 devkit (no display hardware)

Identified 2026-07-03 via esptool + a probe sketch (I²C scan, SD mount attempt,
backlight pin drive) — no touch controller, no SD, no backlight.

| Fact | Value |
| --- | --- |
| Chip | ESP32-D0WD-V3, revision v3.1 |
| Module | ESP-WROOM-32 |
| Features | WiFi, **Bluetooth Classic** (A2DP-capable ✓), dual core, 240 MHz |
| Flash | 4 MB (manufacturer 0x68, device 0x4016), 3.3 V |
| Crystal | 40 MHz |
| MAC | 20:e7:c8:59:18:88 |
| USB-serial | CH340 (VID:PID 1A86:7523) → `/dev/ttyUSB0` |

**What it's good for:** spare board for anything that doesn't need the screen —
WiFi/NTP/weather fetch, NVS settings, Bluetooth A2DP audio experiments.

### PlatformIO

The real config is `platformio.ini` (env `cyd`, pinned `espressif32@7.0.1` =
Arduino-ESP32 core 3.x / IDF 5.x — **A2DP source verified on this combo,
F5 spike 2026-07-07**; `huge_app.csv` partitions because LVGL+A2DP+MP3
exceeds the default app slot; host tests in env `native`).

### Libraries (checked on PlatformIO registry 2026-07-03)

| Library | lib_deps entry | Version |
| --- | --- | --- |
| LovyanGFX | `lovyan03/LovyanGFX` | 1.2.24 |
| LVGL | `lvgl/lvgl` | 9.5.0 latest; last v8 is **8.4.0** (what we use) |
| ArduinoJson | `bblanchon/ArduinoJson` | 7.4.3 |
| ESP32-A2DP | `https://github.com/pschatzmann/ESP32-A2DP.git#b559fb15` (main) | v1.8.11 fails on core 3.x — see note |
| audio-tools | `https://github.com/pschatzmann/arduino-audio-tools.git#v1.2.5` | not on registry |
| helix MP3 | `https://github.com/pschatzmann/arduino-libhelix.git` | separate dep, required for MP3 |

A2DP notes: needs Bluetooth Classic → plain ESP32 only (✓ both boards).
A2DP *source* is verified on core 3.x (F5 spike, 2026-07-07) using ESP32-A2DP
pinned to `main@b559fb15`: the released `v1.8.11` references
`ESP_A2D_AUDIO_STATE_SUSPEND`, which is absent from IDF 5.x, so it won't compile
— the fix is only on main (unreleased). No pioarduino fork and NO
`-DA2DP_I2S_AUDIOTOOLS=1` needed for the source path (that flag force-includes
AudioTools.h, which we don't depend on, and breaks the build). The fallback to
`espressif32@6.9.0` (core 2.0.x) documented in the F5 plan was therefore NOT taken.

### LVGL + LovyanGFX gotchas

- ILI9341-class panels want big-endian RGB565: we use the swap-free glue —
  `LV_COLOR_16_SWAP 0` and the flush callback casts to `lgfx::rgb565_t*` so
  LovyanGFX converts during the SPI write (see `docs/DISPLAY.md`).
- `lv_conf.h` in `include/` + `build_flags = -DLV_CONF_INCLUDE_SIMPLE -Iinclude`.
- **No PSRAM**: keep LVGL draw buffers small (we use one 240×30 buffer).

## Probe sketch

A board-fingerprint sketch (I²C scan w/ CST820 reset, SD mount, backlight drive)
lives at the session scratchpad `board-probe/`; rebuild it any time a new board
arrives to confirm which hardware it is.
