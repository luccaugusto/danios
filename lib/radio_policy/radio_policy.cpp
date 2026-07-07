#include "radio_policy.h"

RadioPlan planTransition(RadioState have, RadioState want) {
  if (have == want) return {{RadioAction::None, RadioAction::None}};

  RadioAction stop = RadioAction::None;
  if (have == RadioState::WiFiOn) stop = RadioAction::StopWiFi;
  if (have == RadioState::BtOn) stop = RadioAction::StopBt;

  RadioAction start = RadioAction::None;
  if (want == RadioState::WiFiOn) start = RadioAction::StartWiFi;
  if (want == RadioState::BtOn) start = RadioAction::StartBt;

  // Teardown always precedes bringup — both radios never coexist (no PSRAM).
  if (stop != RadioAction::None) return {{stop, start}};
  return {{start, RadioAction::None}};
}
