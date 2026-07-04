# Display — the essentials

Everything apps need to know to render on the danios device.
Config lives in `include/LGFX_ESP32_2432S024.hpp` (LovyanGFX). Don't re-derive it.

## The one thing that matters

The panel controller is a **landscape-native 320×240 clone**, not the portrait
ILI9341 the board is sold as. The config therefore declares
`panel/memory = 320×240`, and the app-facing orientation is:

```cpp
LGFX tft;
tft.init();
tft.setRotation(7);   // portrait, USB-C down — 240 wide × 320 tall
tft.setBrightness(160);
```

**All apps render at 240×320 portrait with `setRotation(7)`.** Origin (0,0) is
top-left with the USB-C port at the bottom. `tft.width()` = 240,
`tft.height()` = 320.

## Facts

| What | Value |
| --- | --- |
| Board | ESP32-2432S024 ("Cheap Yellow Display" 2.4" **resistive** variant), ESP32-WROOM-32 |
| Display bus | HSPI — SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, no RST, backlight PWM on 27 |
| Colors | RGB565, `rgb_order = true` (RGB), `invert = false` — both confirmed by eye |
| SD card | separate bus (VSPI, CS 5) |
| Touch | XPT2046 resistive on the **same HSPI bus** as the display (CS 33, PENIRQ 36) — pins + calibration in `docs/hardware.md` |

## Gotchas (hard-won, don't rediscover)

- **Never change the 320×240 panel/memory dims.** Configured as 240×320 the
  writes clip to 75% of the panel and wrap modulo 240. Full story in
  `docs/HANDOVER.md`.
- **`offset_rotation` must stay 0–3** (it's 0). Values 4–7 corrupt the write
  window in LovyanGFX.
- Reference configs (Sunton board def, TFT_eSPI setups) are right about
  **pins** but wrong about **orientation** for this clone — they assume
  portrait-native silicon.
- Touch calibration is **measured and solved** — the min>max corner constants
  in `include/LGFX_ESP32_2432S024.hpp` map raw readings to rotation-independent
  panel-native coords, and LovyanGFX's `convertRawXY` applies the active
  rotation on top. Don't re-measure; don't add manual swap/mirror code.
- Chip ID for sanity checks: RDID4 `0xD3` reads zeros (not a real ILI9341);
  RDDID `0x04` = `0x10 0x81 0xD9`.

## LVGL glue (added by F1)

- LVGL **8.4** (v8 API), `include/lv_conf.h`, enabled via
  `-DLV_CONF_INCLUDE_SIMPLE -Iinclude`.
- `src/services/DisplayService.{h,cpp}` owns the `LGFX` instance and the LVGL
  display driver: **one 240×30 draw buffer** (14.4 KB, static — no PSRAM on
  this board; a second buffer only pays off if the flush ever goes async).
- **Byte order:** `LV_COLOR_16_SWAP 0`. The flush callback casts LVGL's buffer
  to `lgfx::rgb565_t*` and calls `writePixels()` inside
  `startWrite()/setAddrWindow()/endWrite()` — LovyanGFX converts to the
  panel's big-endian RGB565 during the SPI write. Consequence: **image assets
  stay standard (non-swapped) RGB565** in the LVGL image converter.
- Touch: XPT2046 polled through LovyanGFX (`getTouchRaw` + `convertRawXY` —
  calibration + rotation live in the LGFX config) by
  `src/services/TouchService.{h,cpp}`, debounced (`lib/press_debounce/`), and
  fed to LVGL as a pointer indev. There is no separate mapping code.
