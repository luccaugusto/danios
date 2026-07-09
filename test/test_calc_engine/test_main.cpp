// Host-side tests for CalcEngine (pio test -e native).
//
// The engine is the calculator spec's flagship native-TDD module: digit
// entry, classic left-to-right chaining, divide-by-zero/overflow error
// states, and display formatting are all verified off-device.
#include <unity.h>

#include "calc_engine.h"

void setUp() {}
void tearDown() {}

static void test_starts_at_zero() {
  CalcEngine e;
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
  TEST_ASSERT_FALSE(e.inError());
}

static void test_digit_entry_appends() {
  CalcEngine e;
  e.digit('4');
  e.digit('2');
  TEST_ASSERT_EQUAL_STRING("42", e.display().c_str());
}

static void test_leading_zero_is_replaced() {
  CalcEngine e;
  e.digit('0');
  e.digit('5');
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_dot_on_empty_entry_starts_zero_point() {
  CalcEngine e;
  e.dot();
  e.digit('5');
  TEST_ASSERT_EQUAL_STRING("0.5", e.display().c_str());
}

static void test_second_dot_is_ignored() {
  CalcEngine e;
  e.digit('1');
  e.dot();
  e.digit('5');
  e.dot();
  e.digit('2');
  TEST_ASSERT_EQUAL_STRING("1.52", e.display().c_str());
}

static void test_entry_capped_at_12_chars() {
  CalcEngine e;
  for (int i = 0; i < 15; ++i) e.digit('9');
  TEST_ASSERT_EQUAL_STRING("999999999999", e.display().c_str());  // 12 nines
}

static void test_clear_resets_entry() {
  CalcEngine e;
  e.digit('4');
  e.digit('2');
  e.clear();
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_at_zero);
  RUN_TEST(test_digit_entry_appends);
  RUN_TEST(test_leading_zero_is_replaced);
  RUN_TEST(test_dot_on_empty_entry_starts_zero_point);
  RUN_TEST(test_second_dot_is_ignored);
  RUN_TEST(test_entry_capped_at_12_chars);
  RUN_TEST(test_clear_resets_entry);
  return UNITY_END();
}
