#include <unity.h>

#include <bt_addr.h>

void setUp() {}
void tearDown() {}

void test_parse_valid_uppercase() {
  uint8_t b[6] = {};
  TEST_ASSERT_TRUE(parseBtAddr("A1:B2:C3:D4:E5:F6", b));
  const uint8_t want[6] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, b, 6);
}

void test_parse_accepts_lowercase() {
  uint8_t b[6] = {};
  TEST_ASSERT_TRUE(parseBtAddr("a1:b2:c3:d4:e5:f6", b));
  TEST_ASSERT_EQUAL_UINT8(0xA1, b[0]);
  TEST_ASSERT_EQUAL_UINT8(0xF6, b[5]);
}

void test_parse_rejects_bad_input() {
  uint8_t b[6] = {};
  TEST_ASSERT_FALSE(parseBtAddr("", b));
  TEST_ASSERT_FALSE(parseBtAddr("A1:B2:C3:D4:E5", b));        // too short
  TEST_ASSERT_FALSE(parseBtAddr("A1:B2:C3:D4:E5:F6:07", b));  // too long
  TEST_ASSERT_FALSE(parseBtAddr("A1-B2-C3-D4-E5-F6", b));     // wrong sep
  TEST_ASSERT_FALSE(parseBtAddr("G1:B2:C3:D4:E5:F6", b));     // bad hex
}

void test_format_roundtrip() {
  const uint8_t in[6] = {0x00, 0x1A, 0xFF, 0x0B, 0x9C, 0x5D};
  TEST_ASSERT_EQUAL_STRING("00:1A:FF:0B:9C:5D", formatBtAddr(in).c_str());
  uint8_t back[6] = {};
  TEST_ASSERT_TRUE(parseBtAddr(formatBtAddr(in), back));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(in, back, 6);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_uppercase);
  RUN_TEST(test_parse_accepts_lowercase);
  RUN_TEST(test_parse_rejects_bad_input);
  RUN_TEST(test_format_roundtrip);
  return UNITY_END();
}
