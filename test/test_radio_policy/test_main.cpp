#include <unity.h>

#include <radio_policy/radio_policy.h>

void setUp() {}
void tearDown() {}

static void assertPlan(RadioState have, RadioState want, RadioAction s0,
                       RadioAction s1) {
  RadioPlan p = planTransition(have, want);
  TEST_ASSERT_EQUAL_INT((int)s0, (int)p.steps[0]);
  TEST_ASSERT_EQUAL_INT((int)s1, (int)p.steps[1]);
}

void test_noop_transitions() {
  assertPlan(RadioState::Off, RadioState::Off, RadioAction::None, RadioAction::None);
  assertPlan(RadioState::WiFiOn, RadioState::WiFiOn, RadioAction::None, RadioAction::None);
  assertPlan(RadioState::BtOn, RadioState::BtOn, RadioAction::None, RadioAction::None);
}

void test_bringup_from_off() {
  assertPlan(RadioState::Off, RadioState::WiFiOn, RadioAction::StartWiFi, RadioAction::None);
  assertPlan(RadioState::Off, RadioState::BtOn, RadioAction::StartBt, RadioAction::None);
}

void test_teardown_to_off() {
  assertPlan(RadioState::WiFiOn, RadioState::Off, RadioAction::StopWiFi, RadioAction::None);
  assertPlan(RadioState::BtOn, RadioState::Off, RadioAction::StopBt, RadioAction::None);
}

void test_swap_tears_down_first() {
  // The XOR rule (spec §2.2): teardown ALWAYS precedes bringup.
  assertPlan(RadioState::WiFiOn, RadioState::BtOn, RadioAction::StopWiFi, RadioAction::StartBt);
  assertPlan(RadioState::BtOn, RadioState::WiFiOn, RadioAction::StopBt, RadioAction::StartWiFi);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_noop_transitions);
  RUN_TEST(test_bringup_from_off);
  RUN_TEST(test_teardown_to_off);
  RUN_TEST(test_swap_tears_down_first);
  return UNITY_END();
}
