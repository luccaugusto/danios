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

static void test_paren_opens_at_start() {
  CalcEngine e;
  e.paren();
  TEST_ASSERT_EQUAL_STRING("(", e.display().c_str());
}

static void test_paren_opens_after_operator() {
  CalcEngine e;
  e.digit('2');
  e.op('*');
  e.paren();
  TEST_ASSERT_EQUAL_STRING("2*(", e.display().c_str());
}

static void test_paren_opens_after_open_paren() {
  CalcEngine e;
  e.paren();
  e.paren();
  TEST_ASSERT_EQUAL_STRING("((", e.display().c_str());
}

static void test_paren_closes_after_number_when_unclosed() {
  CalcEngine e;
  e.paren();
  e.digit('5');
  e.paren();
  TEST_ASSERT_EQUAL_STRING("(5)", e.display().c_str());
}

static void test_paren_after_number_without_open_ignored() {
  CalcEngine e;
  e.digit('5');
  e.paren();  // nothing to close, '(' after a digit is invalid — ignored
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_paren_closes_nested_groups() {
  CalcEngine e;
  e.paren();
  e.paren();
  e.digit('2');
  e.paren();
  e.paren();
  TEST_ASSERT_EQUAL_STRING("((2))", e.display().c_str());
}

static void test_digit_after_close_paren_ignored() {
  CalcEngine e;
  e.paren();
  e.digit('5');
  e.paren();
  e.digit('7');  // ")7" is invalid — an operator is required first
  TEST_ASSERT_EQUAL_STRING("(5)", e.display().c_str());
}

static void test_dot_after_close_paren_ignored() {
  CalcEngine e;
  e.paren();
  e.digit('5');
  e.paren();
  e.dot();
  TEST_ASSERT_EQUAL_STRING("(5)", e.display().c_str());
}

static void test_unary_minus_after_open_paren() {
  CalcEngine e;
  e.paren();
  e.op('-');
  e.digit('3');
  TEST_ASSERT_EQUAL_STRING("(-3", e.display().c_str());
}

static void test_non_minus_op_after_open_paren_ignored() {
  CalcEngine e;
  e.paren();
  e.op('+');
  TEST_ASSERT_EQUAL_STRING("(", e.display().c_str());
}

static void test_unary_minus_after_paren_not_replaced() {
  CalcEngine e;
  e.paren();
  e.op('-');
  e.op('*');  // "(*" would be invalid — ignored
  TEST_ASSERT_EQUAL_STRING("(-", e.display().c_str());
}

static void test_percent_rewrites_trailing_number() {
  CalcEngine e;
  e.digit('5');
  e.digit('0');
  e.percent();
  TEST_ASSERT_EQUAL_STRING("0.5", e.display().c_str());
}

static void test_percent_only_touches_trailing_number() {
  CalcEngine e;
  e.digit('2');
  e.digit('0');
  e.digit('0');
  e.op('*');
  e.digit('1');
  e.digit('0');
  e.percent();
  TEST_ASSERT_EQUAL_STRING("200*0.1", e.display().c_str());
}

static void test_percent_ignored_after_operator() {
  CalcEngine e;
  e.digit('5');
  e.op('+');
  e.percent();
  TEST_ASSERT_EQUAL_STRING("5+", e.display().c_str());
}

static void test_percent_ignored_after_close_paren() {
  CalcEngine e;
  e.paren();
  e.digit('5');
  e.paren();
  e.percent();
  TEST_ASSERT_EQUAL_STRING("(5)", e.display().c_str());
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
  RUN_TEST(test_paren_opens_at_start);
  RUN_TEST(test_paren_opens_after_operator);
  RUN_TEST(test_paren_opens_after_open_paren);
  RUN_TEST(test_paren_closes_after_number_when_unclosed);
  RUN_TEST(test_paren_after_number_without_open_ignored);
  RUN_TEST(test_paren_closes_nested_groups);
  RUN_TEST(test_digit_after_close_paren_ignored);
  RUN_TEST(test_dot_after_close_paren_ignored);
  RUN_TEST(test_unary_minus_after_open_paren);
  RUN_TEST(test_non_minus_op_after_open_paren_ignored);
  RUN_TEST(test_unary_minus_after_paren_not_replaced);
  RUN_TEST(test_percent_rewrites_trailing_number);
  RUN_TEST(test_percent_only_touches_trailing_number);
  RUN_TEST(test_percent_ignored_after_operator);
  RUN_TEST(test_percent_ignored_after_close_paren);
  return UNITY_END();
}
