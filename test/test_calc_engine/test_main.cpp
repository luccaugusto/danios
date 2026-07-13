// Host-side tests for CalcEngine (pio test -e native).
//
// Expression-entry engine (2026-07-13 spec): the user types a whole
// expression; input guards keep it always-valid; = evaluates with standard
// precedence. These tests cover the editing layer.
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

static void test_leading_zero_replaced_inside_expression() {
  CalcEngine e;
  e.digit('1');
  e.op('+');
  e.digit('0');
  e.digit('7');
  TEST_ASSERT_EQUAL_STRING("1+7", e.display().c_str());
}

static void test_dot_at_boundary_starts_zero_point() {
  CalcEngine e;
  e.dot();
  e.digit('5');
  TEST_ASSERT_EQUAL_STRING("0.5", e.display().c_str());
}

static void test_second_dot_in_same_number_ignored() {
  CalcEngine e;
  e.digit('1');
  e.dot();
  e.digit('5');
  e.dot();
  e.digit('2');
  TEST_ASSERT_EQUAL_STRING("1.52", e.display().c_str());
}

static void test_dot_allowed_in_each_number() {
  CalcEngine e;
  e.digit('1');
  e.dot();
  e.digit('5');
  e.op('+');
  e.dot();  // boundary after '+' -> "0."
  e.digit('2');
  TEST_ASSERT_EQUAL_STRING("1.5+0.2", e.display().c_str());
}

static void test_op_appends_to_expression() {
  CalcEngine e;
  e.digit('1');
  e.digit('2');
  e.op('+');
  e.digit('3');
  e.digit('4');
  TEST_ASSERT_EQUAL_STRING("12+34", e.display().c_str());
}

static void test_second_operator_replaces_first() {
  CalcEngine e;
  e.digit('1');
  e.digit('2');
  e.op('+');
  e.op('*');
  TEST_ASSERT_EQUAL_STRING("12*", e.display().c_str());
}

static void test_unary_minus_at_start() {
  CalcEngine e;
  e.op('-');
  e.digit('5');
  TEST_ASSERT_EQUAL_STRING("-5", e.display().c_str());
}

static void test_non_minus_op_at_start_ignored() {
  CalcEngine e;
  e.op('+');
  e.op('*');
  e.op('/');
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
}

static void test_unary_minus_not_replaced_by_other_op() {
  CalcEngine e;
  e.op('-');
  e.op('+');  // would make "+" a leading operator — ignored
  TEST_ASSERT_EQUAL_STRING("-", e.display().c_str());
}

static void test_number_capped_at_12_chars() {
  CalcEngine e;
  e.digit('1');
  e.op('+');
  for (int i = 0; i < 15; ++i) e.digit('9');
  TEST_ASSERT_EQUAL_STRING("1+999999999999", e.display().c_str());  // 12 nines
}

static void test_dot_counts_toward_number_cap() {
  CalcEngine e;
  for (int i = 0; i < 11; ++i) e.digit('9');
  e.dot();                      // 12th char of the number
  e.digit('5');                 // would be 13th — ignored
  TEST_ASSERT_EQUAL_STRING("99999999999.", e.display().c_str());
}

static void test_expression_capped_at_48_chars() {
  CalcEngine e;
  // "1+1+1+..." — 24 "1+" pairs = 48 chars ending in '+'.
  for (int i = 0; i < 24; ++i) {
    e.digit('1');
    e.op('+');
  }
  TEST_ASSERT_EQUAL_UINT32(48, (uint32_t)e.display().size());
  e.digit('9');  // 49th char — ignored
  TEST_ASSERT_EQUAL_UINT32(48, (uint32_t)e.display().size());
}

static void test_backspace_deletes_across_tokens() {
  CalcEngine e;
  e.digit('1');
  e.digit('2');
  e.op('+');
  e.backspace();
  TEST_ASSERT_EQUAL_STRING("12", e.display().c_str());
  e.backspace();
  TEST_ASSERT_EQUAL_STRING("1", e.display().c_str());
}

static void test_backspace_to_empty_shows_zero() {
  CalcEngine e;
  e.digit('7');
  e.backspace();
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
  e.backspace();  // already empty — no-op
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
}

static void test_clear_resets_expression() {
  CalcEngine e;
  e.digit('4');
  e.op('+');
  e.digit('2');
  e.clear();
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
  TEST_ASSERT_FALSE(e.inError());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_at_zero);
  RUN_TEST(test_digit_entry_appends);
  RUN_TEST(test_leading_zero_is_replaced);
  RUN_TEST(test_leading_zero_replaced_inside_expression);
  RUN_TEST(test_dot_at_boundary_starts_zero_point);
  RUN_TEST(test_second_dot_in_same_number_ignored);
  RUN_TEST(test_dot_allowed_in_each_number);
  RUN_TEST(test_op_appends_to_expression);
  RUN_TEST(test_second_operator_replaces_first);
  RUN_TEST(test_unary_minus_at_start);
  RUN_TEST(test_non_minus_op_at_start_ignored);
  RUN_TEST(test_unary_minus_not_replaced_by_other_op);
  RUN_TEST(test_number_capped_at_12_chars);
  RUN_TEST(test_dot_counts_toward_number_cap);
  RUN_TEST(test_expression_capped_at_48_chars);
  RUN_TEST(test_backspace_deletes_across_tokens);
  RUN_TEST(test_backspace_to_empty_shows_zero);
  RUN_TEST(test_clear_resets_expression);
  return UNITY_END();
}
