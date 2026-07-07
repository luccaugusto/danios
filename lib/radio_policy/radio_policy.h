// danios radio_policy — the WiFi-XOR-Bluetooth rule (spec §2.2) as a pure
// transition table. RadioManager (src/services/) executes the returned steps.
// Pure std C++17; zero Arduino includes.
#pragma once
#include <cstdint>

enum class RadioState : uint8_t { Off, WiFiOn, BtOn };
enum class RadioAction : uint8_t { None, StopBt, StopWiFi, StartWiFi, StartBt };

// Ordered actions (max 2) to reach `want` from `have`. Unused slots = None.
struct RadioPlan {
  RadioAction steps[2];
};

RadioPlan planTransition(RadioState have, RadioState want);
