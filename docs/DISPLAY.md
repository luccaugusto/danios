# Display — the essentials

Everything apps need to know to render on the danios device.
Config lives in `include/LGFX_ESP32_2432S024C.hpp` (LovyanGFX). Don't re-derive it.

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
| Board | ESP32-2432S024C ("Cheap Yellow Display" 2.4" capacitive), ESP32-WROOM-32 |
| Display bus | HSPI — SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, no RST, backlight PWM on 27 |
| Colors | RGB565, `rgb_order = true` (RGB), `invert = false` — both confirmed by eye |
| SD card | separate bus (VSPI, CS 5) — display never shares SPI |
| Touch | CST816S/CST820, I²C SDA 33 / SCL 32, INT 21, RST 25, addr 0x15 (next milestone) |

## Gotchas (hard-won, don't rediscover)

- **Never change the 320×240 panel/memory dims.** Configured as 240×320 the
  writes clip to 75% of the panel and wrap modulo 240. Full story in
  `docs/HANDOVER.md`.
- **`offset_rotation` must stay 0–3** (it's 0). Values 4–7 corrupt the write
  window in LovyanGFX.
- Reference configs (Sunton board def, TFT_eSPI setups) are right about
  **pins** but wrong about **orientation** for this clone — they assume
  portrait-native silicon.
- Expect **touch coordinates to need the same swap + mirror** as the display
  (rotation 7 = MADCTL `MV|MX|MY`). Verify against on-screen targets, not the
  board def's touch flags.
- Chip ID for sanity checks: RDID4 `0xD3` reads zeros (not a real ILI9341);
  RDDID `0x04` = `0x10 0x81 0xD9`.

## NOTE:

when the foundation grows: once LVGL enters the picture, this doc is where its display driver glue (buffer size, flush callback binding to LovyanGFX) should get its few lines
