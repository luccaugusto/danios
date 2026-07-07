// danios RadioManager — sole owner of radio power state (spec §2.2, §3.1).
// Nothing else may touch WiFi.mode()/esp_wifi_*/btStart()/esp_bt_* power.
#pragma once
#include <radio_policy.h>

#include "core/App.h"  // RadioMode

class RadioManager {
 public:
  RadioState current() const { return state_; }

  // Executes teardown-then-bringup per planTransition. Returns false if the
  // bringup step fails; the manager then lands in Off (never half-switched).
  bool request(RadioMode mode);

 private:
  bool execute(RadioAction action);
  bool startWiFi();
  void stopWiFi();
  bool startBt();  // F5 replaces the stub body (returns false until then)
  void stopBt();   // F5 replaces the stub body (no-op until then)

  RadioState state_ = RadioState::Off;
};
