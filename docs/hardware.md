# danios hardware reference

**Last updated:** 2026-07-04

> ⚠️ **2026-07-04 correction:** the CYD unit on hand is the **RESISTIVE**
> 2432S024 variant — XPT2046 on the shared display SPI bus (CS 33, PENIRQ 36),
> **no CST820 / no touch I²C bus at all**. Proven by on-device diagnostics
> (I²C dead on every address, PENIRQ fires on press, XPT2046 returns
> coordinates on the display bus pins). The "Touch — CST820, I²C" section
> below described the C variant we *thought* we had; kept for reference.
> Working touch config lives in `include/LGFX_ESP32_2432S024.hpp`
> (measured calibration) — see `docs/VENDOR-NOTES.md` and
> `.superpowers/sdd/progress.md` for the discovery trail.

## Board currently on hand: bare ESP32 devkit (NOT the CYD)

Identified 2026-07-03 via esptool + a probe sketch (I²C scan on the CYD touch bus,
SD mount attempt, backlight pin drive). No touch controller at 0x15, no SD, no
backlight → generic devkit, no display hardware.

| Fact | Value |
| --- | --- |
| Chip | ESP32-D0WD-V3, revision v3.1 |
| Module | ESP-WROOM-32 |
| Features | WiFi, **Bluetooth Classic** (A2DP-capable ✓), dual core, 240 MHz |
| Flash | 4 MB (manufacturer 0x68, device 0x4016), 3.3 V |
| Crystal | 40 MHz |
| MAC | 20:e7:c8:59:18:88 |
| USB-serial | CH340 (VID:PID 1A86:7523) → `/dev/ttyUSB0` |

**What it's good for:** developing everything that doesn't need the screen —
native unit tests, WiFi/NTP/weather fetch, NVS settings, Bluetooth A2DP audio.

## Target board: ESP32-2432S024C ("CYD" 2.4" capacitive) — **needs to be ordered**

Same chip/module as above, plus display, touch, SD, battery circuit. ~$15;
purchase links in spec §9. Facts below verified 2026-07-03 against two primary
sources for this exact variant:

- [A] https://github.com/edmasini/esp32-2432S024-Capacitive (`hal/esp32/app_hal.h`, `LGFX_ESP32_2432024C.hpp`, `CST820.h`)
- [B] https://github.com/rzeldent/platformio-espressif32-sunton (`esp32-2432S024C.json`)

### Display — ILI9341, 240×320, on HSPI

| Signal | GPIO |
| --- | --- |
| SCLK | 14 |
| MOSI | 13 |
| MISO | 12 |
| CS | 15 |
| DC | 2 |
| RST | none (-1) |
| Backlight | 27 (PWM; [A] uses channel 7 @ 44.1 kHz) |

- SPI: HSPI_HOST, mode 0, 55 MHz write / 20 MHz read proven with LovyanGFX [A].
- `invert = false`; **BGR** color order (`rgb_order = false` in LovyanGFX).
- Native portrait 240×320. Quirk: [B] needs mirror-X in native orientation; [A]
  sets `panel_width/height = 320×320` as a rotation workaround — if rotations
  1/3 show an 80 px offset, that's why. Test all rotations you plan to use early.

### Touch (ACTUAL, this unit) — XPT2046 resistive, shared display SPI

| Signal | GPIO |
| --- | --- |
| SCLK / MOSI / MISO | 14 / 13 / 12 (same as display) |
| CS | 33 |
| PENIRQ | 36 (low while pressed; 10k pull-up on board) |

- LovyanGFX `Touch_XPT2046`, `spi_host = SPI2_HOST`, `bus_shared = true`, 1 MHz.
- Measured calibration (raw 12-bit, portrait USB-down): x_min=3680, x_max=650,
  y_min=580, y_max=3430 (min>max inverts the axis). Raw X grows toward screen
  bottom, raw Y toward screen left.
- Resistive = needs firm/pointed presses; no multitouch, no gestures.

### Touch — CST820, I²C (⚠️ C-variant only — NOT this unit)

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
  *likely* works but is unverified on this exact board.

### microSD — separate bus (VSPI defaults)

| Signal | GPIO |
| --- | --- |
| SCK | 18 |
| MISO | 19 |
| MOSI | 23 |
| CS | 5 |

Arduino: `SD.begin(5)`. Display (HSPI), touch (I²C), SD (VSPI) are three
separate buses — no sharing issues.

### Other peripherals

- RGB LED: R=4, G=16, B=17 (active-low, polarity not documented upstream).
- Photoresistor (CDS): GPIO 34.
- Speaker amp (8002-class mono): GPIO 26.
- **No battery-voltage ADC.** The IP5603 handles charge/boost only; battery %
  is NOT measurable on this board (spec §3.3/§6.4 can't show a real percentage).

### PlatformIO

```ini
[env:esp32]
platform = espressif32          ; official platform = Arduino core 2.0.x —
board = esp32dev                ; the verified combo for A2DP source
framework = arduino
board_build.partitions = huge_app.csv   ; required: LVGL+A2DP+MP3 exceeds default app slot
monitor_speed = 115200
```

Alternative: add [B] as git submodule in `boards/` and use `board = esp32-2432S024C`
to get all pin `#define`s for free.

### Libraries (checked on PlatformIO registry 2026-07-03)

| Library | lib_deps entry | Version |
| --- | --- | --- |
| LovyanGFX | `lovyan03/LovyanGFX` | 1.2.24 |
| LVGL | `lvgl/lvgl` | 9.5.0 latest; last v8 is **8.4.0** |
| ArduinoJson | `bblanchon/ArduinoJson` | 7.4.3 |
| ESP32-A2DP | `https://github.com/pschatzmann/ESP32-A2DP.git#v1.8.11` | not on registry |
| audio-tools | `https://github.com/pschatzmann/arduino-audio-tools.git#v1.2.5` | not on registry |
| helix MP3 | `https://github.com/pschatzmann/arduino-libhelix.git` | separate dep, required for MP3 |

A2DP notes: needs Bluetooth Classic → plain ESP32 only (✓ both boards). With
Arduino core 3.x the legacy I2S API is gone (`-DA2DP_I2S_AUDIOTOOLS=1` +
pioarduino fork needed) — staying on official `espressif32` (core 2.0.x) avoids all that.

### LVGL + LovyanGFX gotchas

- ILI9341 wants big-endian RGB565: either `LV_COLOR_16_SWAP 1`, or swap-free
  config with `writePixels((lgfx::rgb565_t*)&color->full, n)` /
  `setSwapBytes(true)` + `pushImage` in the flush callback.
- `lv_conf.h` in `include/` + `build_flags = -DLV_CONF_INCLUDE_SIMPLE -Iinclude`.
- **No PSRAM**: keep LVGL draw buffers ≤ ~¼ screen ([A] uses 2× 240×30).

## Probe sketch

A board-fingerprint sketch (I²C scan w/ CST820 reset, SD mount, backlight drive)
lives at the session scratchpad `board-probe/`; rebuild it any time a new board
arrives to confirm which hardware it is.
