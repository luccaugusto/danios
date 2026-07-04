// Host-side tests for PressDebounce (pio test -e native).
//
// Why this exists: the XPT2046's pressure/median filter can reject a single
// sample mid-press; without debouncing, LVGL sees PRESSED->RELEASED->PRESSED
// inside the click timeout and fires CLICKED twice per physical tap.
// PressDebounce holds the pressed state through short dropouts.
#include <unity.h>

#include "press_debounce.h"

void setUp() {}
void tearDown() {}

static void test_starts_released() {
  PressDebounce d;
  TEST_ASSERT_FALSE(d.update(false));
}

static void test_press_reported_immediately() {
  PressDebounce d;
  TEST_ASSERT_TRUE(d.update(true));
}

static void test_single_dropout_mid_press_stays_pressed() {
  PressDebounce d;  // default: tolerate 1 missed poll
  d.update(true);
  TEST_ASSERT_TRUE(d.update(false));  // the filter rejected one sample
  TEST_ASSERT_TRUE(d.update(true));   // finger was there all along
}

static void test_two_consecutive_dropouts_release() {
  PressDebounce d;
  d.update(true);
  TEST_ASSERT_TRUE(d.update(false));
  TEST_ASSERT_FALSE(d.update(false));  // real lift
}

static void test_release_then_new_press() {
  PressDebounce d;
  d.update(true);
  d.update(false);
  d.update(false);  // released
  TEST_ASSERT_TRUE(d.update(true));
  TEST_ASSERT_TRUE(d.update(true));
}

static void test_dropout_counter_resets_on_press() {
  PressDebounce d;
  d.update(true);
  d.update(false);  // one miss...
  d.update(true);   // ...absorbed
  TEST_ASSERT_TRUE(d.update(false));  // a fresh single miss still tolerated
  TEST_ASSERT_FALSE(d.update(false));
}

static void test_zero_hold_releases_on_first_miss() {
  PressDebounce d(0);
  d.update(true);
  TEST_ASSERT_FALSE(d.update(false));
}

static void test_stays_released_while_idle() {
  PressDebounce d;
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT_FALSE(d.update(false));
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_released);
  RUN_TEST(test_press_reported_immediately);
  RUN_TEST(test_single_dropout_mid_press_stays_pressed);
  RUN_TEST(test_two_consecutive_dropouts_release);
  RUN_TEST(test_release_then_new_press);
  RUN_TEST(test_dropout_counter_resets_on_press);
  RUN_TEST(test_zero_hold_releases_on_first_miss);
  RUN_TEST(test_stays_released_while_idle);
  return UNITY_END();
}
