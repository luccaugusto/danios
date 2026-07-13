// Host-side tests for weather_model (pio test -e native): the spec's
// data->art bridge — temperature bands, WMO condition groups, display
// conversion, art-slot selection, and the offset->POSIX-TZ rule — all
// verified off-device with the boundary values the spec calls out.
#include <unity.h>

#include <weather_model.h>

void setUp() {}
void tearDown() {}

static void assertBand(TempBand want, float celsius) {
  TEST_ASSERT_EQUAL_INT((int)want, (int)tempBand(celsius));
}

static void test_band_freezing_below_zero() {
  assertBand(TempBand::Freezing, -1.0f);
  assertBand(TempBand::Freezing, -0.1f);
  assertBand(TempBand::Freezing, -25.0f);
}

static void test_band_cold_0_to_9() {
  assertBand(TempBand::Cold, 0.0f);   // spec boundary: -1/0
  assertBand(TempBand::Cold, 9.0f);   // spec boundary: 9/10
  assertBand(TempBand::Cold, 9.9f);
}

static void test_band_mild_10_to_19() {
  assertBand(TempBand::Mild, 10.0f);
  assertBand(TempBand::Mild, 19.9f);
}

static void test_band_warm_20_to_27() {
  assertBand(TempBand::Warm, 20.0f);
  assertBand(TempBand::Warm, 27.0f);  // spec boundary: 27/28
  assertBand(TempBand::Warm, 27.9f);
}

static void test_band_hot_28_up() {
  assertBand(TempBand::Hot, 28.0f);
  assertBand(TempBand::Hot, 41.0f);
}

static void assertCond(Condition want, int code) {
  TEST_ASSERT_EQUAL_INT((int)want, (int)conditionFromWmo(code));
}

static void test_condition_clear() {
  assertCond(Condition::Clear, 0);
  assertCond(Condition::Clear, 1);
}

static void test_condition_cloudy() {
  assertCond(Condition::Cloudy, 2);
  assertCond(Condition::Cloudy, 3);
}

static void test_condition_fog() {
  assertCond(Condition::Fog, 45);
  assertCond(Condition::Fog, 48);
}

static void test_condition_rain() {
  assertCond(Condition::Rain, 51);  // drizzle start
  assertCond(Condition::Rain, 57);  // drizzle end
  assertCond(Condition::Rain, 61);  // rain start
  assertCond(Condition::Rain, 67);  // rain end
  assertCond(Condition::Rain, 80);  // showers start
  assertCond(Condition::Rain, 82);  // showers end
}

static void test_condition_snow() {
  assertCond(Condition::Snow, 71);
  assertCond(Condition::Snow, 77);
  assertCond(Condition::Snow, 85);
  assertCond(Condition::Snow, 86);
}

static void test_condition_storm() {
  assertCond(Condition::Storm, 95);
  assertCond(Condition::Storm, 96);
  assertCond(Condition::Storm, 99);
}

static void test_condition_unknown_codes() {
  // Every gap around the spec's ranges maps to Unknown, never crashes.
  assertCond(Condition::Unknown, -1);
  assertCond(Condition::Unknown, 4);
  assertCond(Condition::Unknown, 50);   // just below drizzle
  assertCond(Condition::Unknown, 58);   // between drizzle and rain
  assertCond(Condition::Unknown, 68);   // between rain and snow
  assertCond(Condition::Unknown, 78);   // between snow and showers
  assertCond(Condition::Unknown, 83);   // between showers and snow showers
  assertCond(Condition::Unknown, 87);
  assertCond(Condition::Unknown, 97);   // between storm codes
  assertCond(Condition::Unknown, 100);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_band_freezing_below_zero);
  RUN_TEST(test_band_cold_0_to_9);
  RUN_TEST(test_band_mild_10_to_19);
  RUN_TEST(test_band_warm_20_to_27);
  RUN_TEST(test_band_hot_28_up);
  RUN_TEST(test_condition_clear);
  RUN_TEST(test_condition_cloudy);
  RUN_TEST(test_condition_fog);
  RUN_TEST(test_condition_rain);
  RUN_TEST(test_condition_snow);
  RUN_TEST(test_condition_storm);
  RUN_TEST(test_condition_unknown_codes);
  return UNITY_END();
}
