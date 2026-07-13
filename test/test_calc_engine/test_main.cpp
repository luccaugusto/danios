// Host-side tests for CalcEngine (pio test -e native).
//
// Expression-entry engine (2026-07-13 spec): the user types a whole
// expression; input guards keep it always-valid; = evaluates with standard
// precedence. These tests cover the editing layer.
#include <unity.h>

#include "calc_engine.h"

void setUp() {}
void tearDown() {}

// Feeds a whole key sequence; '(' and ')' both go through the smart paren()
// key (the keypad only has one), '%' -> percent(), '=' -> equals().
static void type(CalcEngine& e, const char* keys) {
  for (const char* k = keys; *k; ++k) {
    if (*k >= '0' && *k <= '9') e.digit(*k);
    else if (*k == '.') e.dot();
    else if (*k == '(' || *k == ')') e.paren();
    else if (*k == '%') e.percent();
    else if (*k == '=') e.equals();
    else e.op(*k);
  }
}

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

static void test_simple_addition() {
  CalcEngine e;
  type(e, "2+3=");
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_standard_precedence() {
  CalcEngine e;
  type(e, "2+3*4=");  // 14, not the old left-to-right 20
  TEST_ASSERT_EQUAL_STRING("14", e.display().c_str());
}

static void test_parens_override_precedence() {
  CalcEngine e;
  type(e, "(10+20)/3=");
  TEST_ASSERT_EQUAL_STRING("10", e.display().c_str());
}

static void test_nested_parens() {
  CalcEngine e;
  type(e, "((2+1)*3)=");
  TEST_ASSERT_EQUAL_STRING("9", e.display().c_str());
}

static void test_unary_minus_evaluates() {
  CalcEngine e;
  type(e, "-5+2=");
  TEST_ASSERT_EQUAL_STRING("-3", e.display().c_str());
}

static void test_unary_minus_inside_parens() {
  CalcEngine e;
  type(e, "2*(-3+1)=");
  TEST_ASSERT_EQUAL_STRING("-4", e.display().c_str());
}

static void test_division_gives_decimal() {
  CalcEngine e;
  type(e, "7/2=");
  TEST_ASSERT_EQUAL_STRING("3.5", e.display().c_str());
}

static void test_integer_result_has_no_decimals() {
  CalcEngine e;
  type(e, "8/4=");
  TEST_ASSERT_EQUAL_STRING("2", e.display().c_str());
}

static void test_double_noise_rounded_away() {
  CalcEngine e;
  type(e, ".1+.2=");  // 0.30000000000000004 must display as 0.3
  TEST_ASSERT_EQUAL_STRING("0.3", e.display().c_str());
}

static void test_auto_close_open_parens() {
  CalcEngine e;
  type(e, "(2+4=");  // auto-closes to (2+4)
  TEST_ASSERT_EQUAL_STRING("6", e.display().c_str());
}

static void test_trailing_operator_dropped() {
  CalcEngine e;
  type(e, "5+=");
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_trailing_open_paren_and_op_dropped() {
  CalcEngine e;
  type(e, "2+(=");  // strips "(" then "+" -> evaluates "2"
  TEST_ASSERT_EQUAL_STRING("2", e.display().c_str());
}

static void test_degenerate_expression_is_noop() {
  CalcEngine e;
  type(e, "(=");  // no digits — expression stays as typed
  TEST_ASSERT_EQUAL_STRING("(", e.display().c_str());
}

static void test_equals_on_empty_is_noop() {
  CalcEngine e;
  e.equals();
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
  TEST_ASSERT_FALSE(e.inError());
}

static void test_repeated_equals_is_idempotent() {
  CalcEngine e;
  type(e, "2+3==");
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_divide_by_zero_shows_error() {
  CalcEngine e;
  type(e, "5/0=");
  TEST_ASSERT_EQUAL_STRING("Erro", e.display().c_str());
  TEST_ASSERT_TRUE(e.inError());
}

static void test_divide_by_zero_inside_parens() {
  CalcEngine e;
  type(e, "1+(3/0)=");
  TEST_ASSERT_TRUE(e.inError());
}

static void test_error_state_ignores_keys() {
  CalcEngine e;
  type(e, "5/0=");
  type(e, "7+(.%=");  // every editing/eval key must be ignored
  e.backspace();
  TEST_ASSERT_EQUAL_STRING("Erro", e.display().c_str());
}

static void test_clear_recovers_from_error() {
  CalcEngine e;
  type(e, "5/0=");
  e.clear();
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
  type(e, "1+1=");
  TEST_ASSERT_EQUAL_STRING("2", e.display().c_str());
}

static void test_operator_continues_from_result() {
  CalcEngine e;
  type(e, "2+3=");
  type(e, "*2=");  // 5*2
  TEST_ASSERT_EQUAL_STRING("10", e.display().c_str());
}

static void test_digit_after_result_starts_fresh() {
  CalcEngine e;
  type(e, "2+3=");
  e.digit('7');
  TEST_ASSERT_EQUAL_STRING("7", e.display().c_str());
}

static void test_dot_after_result_starts_fresh() {
  CalcEngine e;
  type(e, "2+3=");
  e.dot();
  TEST_ASSERT_EQUAL_STRING("0.", e.display().c_str());
}

static void test_paren_after_result_starts_fresh() {
  CalcEngine e;
  type(e, "2+3=");
  e.paren();
  TEST_ASSERT_EQUAL_STRING("(", e.display().c_str());
}

static void test_backspace_after_result_edits_it() {
  CalcEngine e;
  type(e, "2+3=");
  e.backspace();
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());  // "5" -> "" -> shows 0
}

static void test_percent_then_equals() {
  CalcEngine e;
  type(e, "200*10%=");  // 200*0.1
  TEST_ASSERT_EQUAL_STRING("20", e.display().c_str());
}

static void test_result_never_minus_zero() {
  CalcEngine e;
  type(e, "0*-1=");  // op-replacement turns this into "0-1="? No:
  // '*' after '0' appends, '-' replaces '*' -> "0-", then "1=" -> -1.
  TEST_ASSERT_EQUAL_STRING("-1", e.display().c_str());
  e.clear();
  type(e, "(-0)=");  // unary minus on zero — must not display "-0"
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
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
  RUN_TEST(test_simple_addition);
  RUN_TEST(test_standard_precedence);
  RUN_TEST(test_parens_override_precedence);
  RUN_TEST(test_nested_parens);
  RUN_TEST(test_unary_minus_evaluates);
  RUN_TEST(test_unary_minus_inside_parens);
  RUN_TEST(test_division_gives_decimal);
  RUN_TEST(test_integer_result_has_no_decimals);
  RUN_TEST(test_double_noise_rounded_away);
  RUN_TEST(test_auto_close_open_parens);
  RUN_TEST(test_trailing_operator_dropped);
  RUN_TEST(test_trailing_open_paren_and_op_dropped);
  RUN_TEST(test_degenerate_expression_is_noop);
  RUN_TEST(test_equals_on_empty_is_noop);
  RUN_TEST(test_repeated_equals_is_idempotent);
  RUN_TEST(test_divide_by_zero_shows_error);
  RUN_TEST(test_divide_by_zero_inside_parens);
  RUN_TEST(test_error_state_ignores_keys);
  RUN_TEST(test_clear_recovers_from_error);
  RUN_TEST(test_operator_continues_from_result);
  RUN_TEST(test_digit_after_result_starts_fresh);
  RUN_TEST(test_dot_after_result_starts_fresh);
  RUN_TEST(test_paren_after_result_starts_fresh);
  RUN_TEST(test_backspace_after_result_edits_it);
  RUN_TEST(test_percent_then_equals);
  RUN_TEST(test_result_never_minus_zero);
  return UNITY_END();
}
