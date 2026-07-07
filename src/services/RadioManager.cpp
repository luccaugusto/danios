#include "RadioManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace {
RadioState wanted(RadioMode mode) {
  switch (mode) {
    case RadioMode::WiFi:      return RadioState::WiFiOn;
    case RadioMode::Bluetooth: return RadioState::BtOn;
    default:                   return RadioState::Off;
  }
}
}  // namespace

bool RadioManager::request(RadioMode mode) {
  const RadioState want = wanted(mode);
  const RadioPlan plan = planTransition(state_, want);

  for (RadioAction step : plan.steps) {
    if (step == RadioAction::None) continue;
    if (!execute(step)) {
      // Bringup failed after teardown already ran: we are radio-less.
      state_ = RadioState::Off;
      Serial.printf("[radio] request failed at step %d, heap=%u\n",
                    static_cast<int>(step), esp_get_free_heap_size());
      return false;
    }
  }
  state_ = want;
  Serial.printf("[radio] state=%d heap=%u\n", static_cast<int>(state_),
                esp_get_free_heap_size());
  return true;
}

bool RadioManager::execute(RadioAction action) {
  switch (action) {
    case RadioAction::StartWiFi: return startWiFi();
    case RadioAction::StopWiFi:  stopWiFi(); return true;
    case RadioAction::StartBt:   return startBt();
    case RadioAction::StopBt:    stopBt(); return true;
    default:                     return true;
  }
}

bool RadioManager::startWiFi() { return WiFi.mode(WIFI_STA); }

void RadioManager::stopWiFi() {
  WiFi.disconnect(true /*wifioff*/, true /*eraseap — RAM only, NVS creds are ours*/);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  // F5: Bluetooth (~64 KB) may need esp_wifi_deinit() here to free enough SRAM.
}

// F5: BluetoothAudioService power hooks graft in here. Until then the BT arm
// is a stub so request(Bluetooth) returns false with state Off (roadmap §4.6).
bool RadioManager::startBt() { return false; }  // F5
void RadioManager::stopBt() {}                  // F5
