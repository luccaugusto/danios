// LovyanGFX display config for the ESP32-2432S024C ("Cheap Yellow Display",
// 2.4" capacitive variant).
//
// Pins from the Sunton board definition
// (rzeldent/platformio-espressif32-sunton), cross-checked against
// edmasini/esp32-2432S024-Capacitive:
//   Display: ILI9341 over HSPI  — SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, RST none
//   Backlight (PWM): GPIO 27
//   Color order: BGR
// Touch (CST816S/CST820 on I2C SDA 33 / SCL 32) is intentionally NOT configured
// here yet — that's the next bring-up step after the display is confirmed.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;

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
      cfg.bus_shared       = false;  // TFT has its own SPI bus
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
    setPanel(&_panel);
  }
};
