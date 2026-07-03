// danios — milestone 1: display bring-up smoke test.
// Confirms the toolchain builds, the board flashes, and the display config is
// correct. Draws a centered greeting plus corner markers so orientation is
// unambiguous before building anything on top.
//
// Orientation: the controller is landscape-native 320x240 (see the config
// header), so portrait USB-C-down is the axis-swapped rotation 7 —
// confirmed by eye on 2026-07-03: full-panel teal, red top-left, blue
// bottom-right, text upright with the USB-C port at the bottom.
#include <Arduino.h>
#include "LGFX_ESP32_2432S024C.hpp"

LGFX tft;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[danios] display bring-up starting");

  tft.init();
  tft.setRotation(7);  // portrait, USB-C down (240x320)
  tft.setBrightness(160);

  // Solid teal fill (not black) so any reflection on the glass is overpowered,
  // and a 1px white border so we can confirm the whole panel is addressed edge
  // to edge (no unwritten strip).
  const uint16_t BG = tft.color565(0, 64, 96);
  tft.fillScreen(BG);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_WHITE);

  // Corner markers so orientation is unambiguous:
  //   red square = top-left, blue square = bottom-right.
  tft.fillRect(0, 0, 30, 30, TFT_RED);
  tft.fillRect(tft.width() - 30, tft.height() - 30, 30, 30, TFT_BLUE);

  tft.setTextColor(TFT_WHITE, BG);
  tft.setTextDatum(middle_center);
  tft.setTextSize(2);
  tft.drawString("hello danios", tft.width() / 2, tft.height() / 2 - 16);

  tft.setTextSize(1);
  tft.drawString(String(tft.width()) + " x " + String(tft.height()),
                 tft.width() / 2, tft.height() / 2 + 16);

  Serial.printf("[danios] display up: %d x %d\n", tft.width(), tft.height());
}

void loop() {
  // Heartbeat so we can confirm the sketch is running over serial too.
  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.println("[danios] alive");
  }
}
