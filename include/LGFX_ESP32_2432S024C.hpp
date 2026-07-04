// LovyanGFX display + touch config for the ESP32-2432S024 ("Cheap Yellow
// Display", 2.4"). Filename says S024C, but Task 6 diagnostics proved this
// unit is the RESISTIVE variant: XPT2046 on the shared display SPI bus, NO
// CST820 (docs/VENDOR-NOTES.md, .superpowers/sdd/progress.md).
//
// Pins per the vendor schematic (reference/2.4inch_ESP32-2432S024/5-Schematic):
//   Display: ILI9341-class over HSPI — SCLK 14, MOSI 13, MISO 12, CS 15, DC 2
//   Backlight (PWM): GPIO 27
//   Touch: XPT2046 on the SAME SPI bus — CS 33, PENIRQ 36
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341   _panel;
  lgfx::Bus_SPI         _bus;
  lgfx::Light_PWM       _light;
  lgfx::Touch_XPT2046   _touch;

public:
  LGFX(void) {
    {  // SPI bus (HSPI — the display's own bus; SD card is on VSPI)
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;   // HSPI on the classic ESP32
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 14;
      cfg.pin_mosi    = 13;
      cfg.pin_miso    = 12;
      cfg.pin_dc      = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {  // ILI9341 panel
      auto cfg = _panel.config();
      cfg.pin_cs           = 15;
      cfg.pin_rst          = -1;   // not wired on this board
      cfg.pin_busy         = -1;
      // The controller on this unit is NOT a genuine portrait ILI9341: it
      // fails the ILI9341 ID check (RDID4 0xD3 reads all zeros) and photos
      // prove its column axis runs along the 320px LONG side — i.e. the
      // silicon is landscape-native (ILI9342-style, 320x240). Configuring it
      // as 240x320 is what caused every partial-fill symptom: even rotations
      // left 80 of 320 columns unpainted (the ~75% fills), and out-of-range
      // row addresses wrap modulo 240. Declare the true geometry and portrait
      // becomes an axis-swapped (odd) rotation with fully valid windows.
      cfg.panel_width      = 320;
      cfg.panel_height     = 240;
      cfg.memory_width     = 320;  // Panel_ILI9341 presets 240x320; override both
      cfg.memory_height    = 240;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      // NOTE: offset_rotation MUST stay in 0..3 — LovyanGFX miscalculates the
      // write window if it's 4..7 (per LovyanGFX maintainer).
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = false;
      cfg.rgb_order        = true;   // this panel is RGB (BGR rendered teal as yellow)
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;  // gates SD-card sharing only (SD is on VSPI);
                                     // the XPT2046 shares this bus but arbitrates
                                     // via the touch config's own bus_shared below
      _panel.config(cfg);
    }
    {  // PWM backlight
      auto cfg = _light.config();
      cfg.pin_bl      = 27;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    {  // XPT2046 resistive touch, sharing the display's HSPI bus.
      auto cfg = _touch.config();
      // Calibration + axis mapping measured on this unit (2026-07-04 diag,
      // ledger step 4): raw X grows toward the portrait BOTTOM, raw Y grows
      // toward the portrait LEFT. min>max is intentional — it inverts that
      // axis. These corners map raw -> rotation-INDEPENDENT panel-native
      // 320x240 coords; convertRawXY applies the active rotation on top at
      // call time, so the constants stay valid if setRotation ever changes.
      cfg.x_min      = 3680;  // raw X at portrait bottom -> panel x 0
      cfg.x_max      = 650;   // raw X at portrait top    -> panel x 319
      cfg.y_min      = 580;   // raw Y at portrait right  -> panel y 0
      cfg.y_max      = 3430;  // raw Y at portrait left   -> panel y 239
      cfg.offset_rotation = 0;
      cfg.pin_int    = 36;    // PENIRQ — driver skips SPI traffic when idle
      cfg.bus_shared = true;  // same bus as the panel: pause its transaction
      cfg.spi_host   = SPI2_HOST;
      cfg.freq       = 1000000;
      cfg.pin_sclk   = 14;
      cfg.pin_mosi   = 13;
      cfg.pin_miso   = 12;
      cfg.pin_cs     = 33;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};
