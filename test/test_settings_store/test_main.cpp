#include <unity.h>
#include <settings_store.h>

void setUp() {}
void tearDown() {}

void test_defaults_returned_when_key_missing() {
  FakeSettingsStore s;
  TEST_ASSERT_EQUAL_UINT32(160u, s.getU32("disp.bright", 160u));
  TEST_ASSERT_EQUAL_INT32(-5, s.getI32("nope", -5));
  TEST_ASSERT_EQUAL_FLOAT(1.5f, s.getFloat("nope", 1.5f));
  TEST_ASSERT_FALSE(s.getBool("units.f", false));
  TEST_ASSERT_TRUE(s.getBool("nope2", true));
  TEST_ASSERT_EQUAL_STRING("UTC0", s.getString("tz", "UTC0").c_str());
}

void test_u32_roundtrip() {
  FakeSettingsStore s;
  s.setU32("disp.bright", 200u);
  TEST_ASSERT_EQUAL_UINT32(200u, s.getU32("disp.bright", 160u));
}

void test_i32_roundtrip_negative() {
  FakeSettingsStore s;
  s.setI32("pet.disc", -3);
  TEST_ASSERT_EQUAL_INT32(-3, s.getI32("pet.disc", 0));
}

void test_float_roundtrip() {
  FakeSettingsStore s;
  s.setFloat("loc.lat", -23.55f);
  TEST_ASSERT_EQUAL_FLOAT(-23.55f, s.getFloat("loc.lat", 0.0f));
}

void test_bool_roundtrip() {
  FakeSettingsStore s;
  s.setBool("units.f", true);
  TEST_ASSERT_TRUE(s.getBool("units.f", false));
  s.setBool("units.f", false);
  TEST_ASSERT_FALSE(s.getBool("units.f", true));
}

void test_string_roundtrip() {
  FakeSettingsStore s;
  s.setString("wifi.ssid", "HomeNet");
  TEST_ASSERT_EQUAL_STRING("HomeNet", s.getString("wifi.ssid", "").c_str());
}

void test_overwrite_replaces_value() {
  FakeSettingsStore s;
  s.setU32("disp.sleep_s", 30u);
  s.setU32("disp.sleep_s", 300u);
  TEST_ASSERT_EQUAL_UINT32(300u, s.getU32("disp.sleep_s", 60u));
}

void test_remove_erases_key_for_all_types() {
  FakeSettingsStore s;
  s.setU32("k", 9u);
  s.setString("k", "nine");
  s.setBool("k", true);
  s.remove("k");
  TEST_ASSERT_EQUAL_UINT32(1u, s.getU32("k", 1u));
  TEST_ASSERT_EQUAL_STRING("d", s.getString("k", "d").c_str());
  TEST_ASSERT_FALSE(s.getBool("k", false));
}

void test_distinct_typed_keys_coexist() {
  FakeSettingsStore s;
  s.setU32("disp.bright", 42u);
  s.setBool("units.f", true);
  s.setString("wifi.ssid", "net");
  s.setFloat("loc.lon", 1.25f);
  s.setI32("pet.disc", -1);
  TEST_ASSERT_EQUAL_UINT32(42u, s.getU32("disp.bright", 0u));
  TEST_ASSERT_TRUE(s.getBool("units.f", false));
  TEST_ASSERT_EQUAL_STRING("net", s.getString("wifi.ssid", "").c_str());
  TEST_ASSERT_EQUAL_FLOAT(1.25f, s.getFloat("loc.lon", 0.0f));
  TEST_ASSERT_EQUAL_INT32(-1, s.getI32("pet.disc", 0));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_defaults_returned_when_key_missing);
  RUN_TEST(test_u32_roundtrip);
  RUN_TEST(test_i32_roundtrip_negative);
  RUN_TEST(test_float_roundtrip);
  RUN_TEST(test_bool_roundtrip);
  RUN_TEST(test_string_roundtrip);
  RUN_TEST(test_overwrite_replaces_value);
  RUN_TEST(test_remove_erases_key_for_all_types);
  RUN_TEST(test_distinct_typed_keys_coexist);
  return UNITY_END();
}
