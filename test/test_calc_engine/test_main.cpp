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

static void test_addition() {
  CalcEngine e;
  e.digit('2');
  e.op('+');
  e.digit('3');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_subtraction() {
  CalcEngine e;
  e.digit('9');
  e.op('-');
  e.digit('4');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_multiplication() {
  CalcEngine e;
  e.digit('6');
  e.op('*');
  e.digit('7');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("42", e.display().c_str());
}

static void test_division_no_trailing_zeros() {
  CalcEngine e;
  e.digit('7');
  e.op('/');
  e.digit('2');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("3.5", e.display().c_str());  // not "3.500000"
}

// The spec's own example: 2 + 3 × 4 = → 20 (left-to-right, not precedence).
static void test_chaining_evaluates_left_to_right() {
  CalcEngine e;
  e.digit('2');
  e.op('+');
  e.digit('3');
  e.op('*');  // evaluates 2+3 here → display shows 5
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
  e.digit('4');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("20", e.display().c_str());
}

static void test_second_operator_replaces_first() {
  CalcEngine e;
  e.digit('6');
  e.op('+');
  e.op('*');  // changed my mind: multiply, not add
  e.digit('7');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("42", e.display().c_str());
}

static void test_continue_from_result() {
  CalcEngine e;
  e.digit('2');
  e.op('+');
  e.digit('3');
  e.equals();  // 5
  e.op('+');
  e.digit('4');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("9", e.display().c_str());
}

static void test_equals_without_operator_keeps_entry() {
  CalcEngine e;
  e.digit('5');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_integer_result_has_no_decimals() {
  CalcEngine e;
  e.digit('8');
  e.op('/');
  e.digit('4');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("2", e.display().c_str());  // not "2.000000"
}

static void test_double_noise_rounded_away() {
  CalcEngine e;
  e.dot(); e.digit('1');   // 0.1
  e.op('+');
  e.dot(); e.digit('2');   // 0.2
  e.equals();
  TEST_ASSERT_EQUAL_STRING("0.3", e.display().c_str());  // not 0.30000000000000004
}

static void test_divide_by_zero_shows_error() {
  CalcEngine e;
  e.digit('5');
  e.op('/');
  e.digit('0');
  e.equals();
  TEST_ASSERT_TRUE(e.inError());
  TEST_ASSERT_EQUAL_STRING("Erro", e.display().c_str());  // never raw inf/nan
}

static void test_divide_by_zero_via_chaining() {
  CalcEngine e;
  e.digit('5');
  e.op('/');
  e.digit('0');
  e.op('+');  // the chained evaluation happens here
  TEST_ASSERT_EQUAL_STRING("Erro", e.display().c_str());
}

static void test_error_state_ignores_keys() {
  CalcEngine e;
  e.digit('5');
  e.op('/');
  e.digit('0');
  e.equals();
  e.digit('7');
  e.dot();
  e.op('+');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("Erro", e.display().c_str());
}

static void test_clear_recovers_from_error() {
  CalcEngine e;
  e.digit('5');
  e.op('/');
  e.digit('0');
  e.equals();
  e.clear();
  TEST_ASSERT_FALSE(e.inError());
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
  e.digit('1');
  e.op('+');
  e.digit('1');
  e.equals();
  TEST_ASSERT_EQUAL_STRING("2", e.display().c_str());  // fully functional again
}

static void test_overflow_sets_error_not_inf() {
  CalcEngine e;
  for (int i = 0; i < 12; ++i) e.digit('9');       // ~1e12
  for (int round = 0; round < 30 && !e.inError(); ++round) {
    e.op('*');
    for (int i = 0; i < 12; ++i) e.digit('9');     // ×~1e12 per round → inf ~round 25
    e.equals();
  }
  TEST_ASSERT_TRUE(e.inError());
  TEST_ASSERT_EQUAL_STRING("Erro", e.display().c_str());
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
  RUN_TEST(test_addition);
  RUN_TEST(test_subtraction);
  RUN_TEST(test_multiplication);
  RUN_TEST(test_division_no_trailing_zeros);
  RUN_TEST(test_chaining_evaluates_left_to_right);
  RUN_TEST(test_second_operator_replaces_first);
  RUN_TEST(test_continue_from_result);
  RUN_TEST(test_equals_without_operator_keeps_entry);
  RUN_TEST(test_integer_result_has_no_decimals);
  RUN_TEST(test_double_noise_rounded_away);
  RUN_TEST(test_divide_by_zero_shows_error);
  RUN_TEST(test_divide_by_zero_via_chaining);
  RUN_TEST(test_error_state_ignores_keys);
  RUN_TEST(test_clear_recovers_from_error);
  RUN_TEST(test_overflow_sets_error_not_inf);
  return UNITY_END();
}
