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

static void test_band_cold_0_to_14() {
  assertBand(TempBand::Cold, 0.0f);   // README boundary: -1/0
  assertBand(TempBand::Cold, 14.0f);  // README boundary: 14/15
  assertBand(TempBand::Cold, 14.9f);
}

static void test_band_mild_15_to_23() {
  assertBand(TempBand::Mild, 15.0f);
  assertBand(TempBand::Mild, 23.9f);
}

static void test_band_warm_24_to_27() {
  assertBand(TempBand::Warm, 24.0f);
  assertBand(TempBand::Warm, 27.0f);  // README boundary: 27/28
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

static void test_celsius_passthrough() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, toDisplayTemp(20.0f, false));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.5f, toDisplayTemp(-3.5f, false));
}

static void test_fahrenheit_conversion() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, toDisplayTemp(0.0f, true));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 212.0f, toDisplayTemp(100.0f, true));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -40.0f, toDisplayTemp(-40.0f, true));
}

static void test_condition_labels_pt() {
  TEST_ASSERT_EQUAL_STRING("Ensolarado", conditionLabelPt(Condition::Clear));
  TEST_ASSERT_EQUAL_STRING("Nublado", conditionLabelPt(Condition::Cloudy));
  TEST_ASSERT_EQUAL_STRING("Neblina", conditionLabelPt(Condition::Fog));
  TEST_ASSERT_EQUAL_STRING("Chuva", conditionLabelPt(Condition::Rain));
  TEST_ASSERT_EQUAL_STRING("Neve", conditionLabelPt(Condition::Snow));
  TEST_ASSERT_EQUAL_STRING("Tempestade", conditionLabelPt(Condition::Storm));
  TEST_ASSERT_EQUAL_STRING("--", conditionLabelPt(Condition::Unknown));
}

static void test_art_outfit_per_band() {
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_freezing.bin",
      artSlots(TempBand::Freezing, Condition::Snow, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_cold.bin",
      artSlots(TempBand::Cold, Condition::Cloudy, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_mild.bin",
      artSlots(TempBand::Mild, Condition::Clear, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_warm.bin",
      artSlots(TempBand::Warm, Condition::Rain, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_hot.bin",
      artSlots(TempBand::Hot, Condition::Clear, true).outfit);
}

static void test_art_overlay_per_condition() {
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_sunglasses.bin",
      artSlots(TempBand::Hot, Condition::Clear, true).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_umbrella.bin",
      artSlots(TempBand::Warm, Condition::Rain, true).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_umbrella.bin",
      artSlots(TempBand::Warm, Condition::Storm, true).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_scarf.bin",
      artSlots(TempBand::Freezing, Condition::Snow, true).overlay);
  TEST_ASSERT_NULL(artSlots(TempBand::Mild, Condition::Cloudy, true).overlay);
  TEST_ASSERT_NULL(artSlots(TempBand::Mild, Condition::Fog, true).overlay);
}

static void test_art_hat_on_mild_clear_or_rain() {
  // Mild band swaps the condition accessory for the hat on sunny/rainy days.
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_hat.bin",
      artSlots(TempBand::Mild, Condition::Clear, true).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_hat.bin",
      artSlots(TempBand::Mild, Condition::Rain, true).overlay);
  // Not at night, and storm keeps the umbrella even when mild.
  TEST_ASSERT_NULL(artSlots(TempBand::Mild, Condition::Clear, false).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_umbrella.bin",
      artSlots(TempBand::Mild, Condition::Storm, true).overlay);
}

static void test_art_background_per_condition() {
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_clear.bin",
      artSlots(TempBand::Warm, Condition::Clear, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_cloudy.bin",
      artSlots(TempBand::Warm, Condition::Cloudy, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_fog.bin",
      artSlots(TempBand::Warm, Condition::Fog, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_rain.bin",
      artSlots(TempBand::Warm, Condition::Rain, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_snow.bin",
      artSlots(TempBand::Cold, Condition::Snow, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_storm.bin",
      artSlots(TempBand::Warm, Condition::Storm, true).background);
}

static void test_art_clear_night_variant() {
  // is_day polish (spec): night gets its own clear sky, and no sunglasses.
  const ArtSlots night = artSlots(TempBand::Mild, Condition::Clear, false);
  TEST_ASSERT_EQUAL_STRING("S:/art/weather/bg_clear_night.bin", night.background);
  TEST_ASSERT_NULL(night.overlay);
}

static void test_art_unknown_condition_has_no_slots() {
  const ArtSlots s = artSlots(TempBand::Mild, Condition::Unknown, true);
  TEST_ASSERT_EQUAL_STRING("S:/art/weather/outfit_mild.bin", s.outfit);
  TEST_ASSERT_NULL(s.overlay);
  TEST_ASSERT_NULL(s.background);
}

static void test_tz_utc_minus_3() {
  // The spec's own example: -10800 -> "<-03>3".
  TEST_ASSERT_EQUAL_STRING("<-03>3", posixTzFromOffset(-10800).c_str());
}

static void test_tz_utc_plus_2() {
  // POSIX offsets are inverted (seconds WEST of UTC): east zones are negative.
  TEST_ASSERT_EQUAL_STRING("<+02>-2", posixTzFromOffset(7200).c_str());
}

static void test_tz_utc_zero() {
  TEST_ASSERT_EQUAL_STRING("<+00>0", posixTzFromOffset(0).c_str());
}

static void test_tz_half_hour_east() {
  // India, UTC+5:30.
  TEST_ASSERT_EQUAL_STRING("<+0530>-5:30", posixTzFromOffset(19800).c_str());
}

static void test_tz_half_hour_west() {
  // Newfoundland, UTC-3:30.
  TEST_ASSERT_EQUAL_STRING("<-0330>3:30", posixTzFromOffset(-12600).c_str());
}

static void test_tz_quarter_hour() {
  // Nepal, UTC+5:45.
  TEST_ASSERT_EQUAL_STRING("<+0545>-5:45", posixTzFromOffset(20700).c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_band_freezing_below_zero);
  RUN_TEST(test_band_cold_0_to_14);
  RUN_TEST(test_band_mild_15_to_23);
  RUN_TEST(test_band_warm_24_to_27);
  RUN_TEST(test_band_hot_28_up);
  RUN_TEST(test_condition_clear);
  RUN_TEST(test_condition_cloudy);
  RUN_TEST(test_condition_fog);
  RUN_TEST(test_condition_rain);
  RUN_TEST(test_condition_snow);
  RUN_TEST(test_condition_storm);
  RUN_TEST(test_condition_unknown_codes);
  RUN_TEST(test_celsius_passthrough);
  RUN_TEST(test_fahrenheit_conversion);
  RUN_TEST(test_condition_labels_pt);
  RUN_TEST(test_art_outfit_per_band);
  RUN_TEST(test_art_overlay_per_condition);
  RUN_TEST(test_art_hat_on_mild_clear_or_rain);
  RUN_TEST(test_art_background_per_condition);
  RUN_TEST(test_art_clear_night_variant);
  RUN_TEST(test_art_unknown_condition_has_no_slots);
  RUN_TEST(test_tz_utc_minus_3);
  RUN_TEST(test_tz_utc_plus_2);
  RUN_TEST(test_tz_utc_zero);
  RUN_TEST(test_tz_half_hour_east);
  RUN_TEST(test_tz_half_hour_west);
  RUN_TEST(test_tz_quarter_hour);
  return UNITY_END();
}
