// F5 Task 0 spike (throwaway): prove ESP32-A2DP works as an A2DP SOURCE on
// espressif32@7.0.1 / arduino-esp32 3.x (roadmap deviation 5.2). Plays a
// 440 Hz sine to a named speaker. If this does not build/run, take the
// fallback in Step 4 — do NOT proceed to Task 1 with an unproven stack.
#include <Arduino.h>
#include <BluetoothA2DPSource.h>

#include <cmath>

static const char* kSpeakerName = "MY-SPEAKER";  // <-- your speaker's name

static BluetoothA2DPSource a2dp;
static float phase = 0.0f;

static int32_t getFrames(Frame* frames, int32_t count) {
  constexpr float kStep = 2.0f * PI * 440.0f / 44100.0f;
  for (int32_t i = 0; i < count; ++i) {
    const int16_t s = static_cast<int16_t>(8000.0f * sinf(phase));
    frames[i].channel1 = s;
    frames[i].channel2 = s;
    phase += kStep;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
  }
  return count;
}

void setup() {
  Serial.begin(115200);
  Serial.printf("[spike] A2DP source starting, heap=%u\n",
                esp_get_free_heap_size());
  a2dp.set_data_callback_in_frames(getFrames);
  a2dp.start(kSpeakerName);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.printf("[spike] connected=%d heap=%u\n", a2dp.is_connected(),
                  esp_get_free_heap_size());
  }
}
