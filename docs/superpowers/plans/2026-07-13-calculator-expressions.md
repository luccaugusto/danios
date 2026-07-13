# Calculator Expression Entry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the calculator's immediate-execution engine with expression
entry — type `(10+20)/3` on one line, `=` evaluates with standard precedence.

**Architecture:** `lib/calc_engine/` is rewritten from an accumulator state
machine into an expression editor (`std::string expr_` kept always-valid by
input guards) plus a recursive-descent evaluator run on `=`. The UI layer
(`CalculatorApp`) only swaps the `+/-` key for a smart `()` key and renders
the engine's ASCII `*`/`/` as `×`/`÷`. Spec:
`docs/superpowers/specs/2026-07-13-calculator-expressions-design.md`.

**Tech Stack:** C++17, Unity tests (`pio test -e native`), LVGL v8 btnmatrix
on the CYD (`pio run -e cyd`).

## Global Constraints

- `lib/calc_engine/` stays pure C++17: zero Arduino/LVGL includes.
- No SD paths, no NVS keys, no radio use.
- Engine strings are ASCII only (`0-9 . + - * / ( )`); the UI substitutes
  display glyphs. Minus is ASCII `-` everywhere (U+2212 not in the font).
- Numbers are capped at 12 chars; the whole expression at 48 chars.
- Error display string is exactly `"Erro"`; only `C` exits the error state.
- Result formatting: `%.10g`, never `-0`.
- Tests: Unity convention as in the existing `test/test_*/test_main.cpp`
  files (static test functions, `RUN_TEST` list in `main`, `setUp`/`tearDown`
  stubs). All calc_engine tests live in `test/test_calc_engine/test_main.cpp`.

---

### Task 1: CalcEngine expression editing — digit, dot, op, backspace, clear

Rewrites the engine's state to `expr_` and implements every editing key
except `paren()`/`percent()` (Task 2) and `equals()` (Task 3). The header
lands in its final form now; later tasks only add function bodies. The
`lastWasResult_` branches written here are inert until Task 3 implements
`equals()` — they compile and are exercised by Task 3's tests.

**Files:**
- Rewrite: `lib/calc_engine/calc_engine.h`
- Rewrite: `lib/calc_engine/calc_engine.cpp`
- Rewrite: `test/test_calc_engine/test_main.cpp`

**Interfaces:**
- Consumes: nothing (fresh rewrite).
- Produces: `class CalcEngine` with `void digit(char)`, `void dot()`,
  `void op(char)`, `void paren()`, `void percent()`, `void equals()`,
  `void backspace()`, `void clear()`, `std::string display() const`,
  `bool inError() const`. Private helpers `size_t lastNumberStart() const`,
  `static int unclosed(const std::string&)`,
  `static std::string format(double)`. Tasks 2–3 add bodies for
  `paren`/`percent`/`equals` in the same .cpp.

- [ ] **Step 1: Write the failing tests (full file replacement)**

Replace `test/test_calc_engine/test_main.cpp` with:

```cpp
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_calc_engine`
Expected: FAIL — the tests compile against the old header (they only use
methods it also had), but expression tests fail at runtime: e.g.
`test_op_appends_to_expression` expects `"12+34"` while the old engine
displays only the current operand (`"34"`), and
`test_unary_minus_at_start` expects `"-5"` while the old engine shows `"5"`.

- [ ] **Step 3: Write the new header**

Replace `lib/calc_engine/calc_engine.h` with:

```cpp
// lib/calc_engine/calc_engine.h — pure calculator expression engine
// (roadmap §3, A1 + 2026-07-13 expression-entry spec). std C++17 only,
// zero Arduino/LVGL includes. The UI wrapper feeds keys in and renders
// display(); all behavior is native-tested.
//
// The user types a whole expression (e.g. "(10+20)/3"); input guards keep
// it always-valid-so-far; equals() evaluates with standard precedence.
#pragma once

#include <string>

class CalcEngine {
 public:
  void digit(char d);   // '0'..'9'
  void dot();           // decimal point (one per number)
  void op(char o);      // '+', '-', '*', '/'; '-' also unary at start/after '('
  void paren();         // smart key: inserts '(' or ')' based on context
  void percent();       // rewrites the trailing number to value/100
  void equals();        // evaluate; result becomes the expression
  void backspace();     // deletes the last character
  void clear();         // C — full reset; the only way out of error state
  std::string display() const;
  bool inError() const { return error_; }

 private:
  size_t lastNumberStart() const;              // index where the trailing number begins
  static int unclosed(const std::string& s);   // '(' minus ')' count
  static std::string format(double v);

  std::string expr_;            // expression as typed; ASCII ops + - * / ( )
  bool lastWasResult_ = false;  // after '=': op continues, digit starts fresh
  bool error_ = false;          // ÷0 / non-finite; display() = "Erro"
};
```

- [ ] **Step 4: Write the editing implementation**

Replace `lib/calc_engine/calc_engine.cpp` with (Tasks 2–3 append the
`paren`/`percent`/`equals` bodies; they are declared but intentionally not
defined yet — nothing references them until their tests exist):

```cpp
#include "calc_engine.h"

#include <cstdio>
#include <cstdlib>

namespace {
constexpr size_t kMaxNumberLen = 12;  // longest typeable number
constexpr size_t kMaxExprLen = 48;    // whole-expression cap

bool isOp(char c) { return c == '+' || c == '-' || c == '*' || c == '/'; }
bool isDigitCh(char c) { return c >= '0' && c <= '9'; }
}  // namespace

void CalcEngine::digit(char d) {
  if (error_) return;
  if (lastWasResult_) {  // typing a digit after '=' starts a fresh expression
    expr_.clear();
    lastWasResult_ = false;
  }
  if (!expr_.empty() && expr_.back() == ')') return;  // needs an operator first
  const size_t numStart = lastNumberStart();
  const size_t numLen = expr_.size() - numStart;
  if (numLen >= kMaxNumberLen) return;
  if (numLen == 1 && expr_[numStart] == '0') {  // "0" then "5" types "5"
    expr_[numStart] = d;
    return;
  }
  if (expr_.size() >= kMaxExprLen) return;
  expr_ += d;
}

void CalcEngine::dot() {
  if (error_) return;
  if (lastWasResult_) {
    expr_.clear();
    lastWasResult_ = false;
  }
  if (!expr_.empty() && expr_.back() == ')') return;
  const size_t numStart = lastNumberStart();
  if (expr_.find('.', numStart) != std::string::npos) return;  // one per number
  const bool atBoundary = (numStart == expr_.size());
  const size_t need = atBoundary ? 2 : 1;  // "." at a boundary types "0."
  if (expr_.size() - numStart + need > kMaxNumberLen) return;
  if (expr_.size() + need > kMaxExprLen) return;
  if (atBoundary) expr_ += '0';
  expr_ += '.';
}

void CalcEngine::op(char o) {
  if (error_) return;
  lastWasResult_ = false;  // an operator after '=' continues from the result
  const bool unaryPos = expr_.empty() || expr_.back() == '(';
  if (unaryPos) {  // only unary minus is valid here
    if (o == '-' && expr_.size() < kMaxExprLen) expr_ += '-';
    return;
  }
  if (isOp(expr_.back())) {
    // Don't rewrite a unary minus ("-" at start / after '(') into "+*/".
    const bool wasUnary =
        expr_.size() == 1 || expr_[expr_.size() - 2] == '(';
    if (wasUnary) return;
    expr_.back() = o;  // a second operator in a row replaces the first
    return;
  }
  if (expr_.size() >= kMaxExprLen) return;
  expr_ += o;
}

void CalcEngine::backspace() {
  if (error_ || expr_.empty()) return;
  expr_.pop_back();
  lastWasResult_ = false;
}

void CalcEngine::clear() {
  expr_.clear();
  lastWasResult_ = false;
  error_ = false;
}

std::string CalcEngine::display() const {
  if (error_) return "Erro";
  if (expr_.empty()) return "0";
  return expr_;
}

size_t CalcEngine::lastNumberStart() const {
  size_t i = expr_.size();
  while (i > 0 && (isDigitCh(expr_[i - 1]) || expr_[i - 1] == '.')) --i;
  return i;
}

int CalcEngine::unclosed(const std::string& s) {
  int n = 0;
  for (char c : s) {
    if (c == '(') ++n;
    else if (c == ')') --n;
  }
  return n;
}

std::string CalcEngine::format(double v) {
  if (v == 0.0) return "0";  // also catches -0.0 → never display "-0"
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.10g", v);
  return buf;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_calc_engine`
Expected: 18 tests, all PASS.

- [ ] **Step 6: Run the full native suite (other test dirs must be untouched)**

Run: `pio test -e native`
Expected: all test dirs PASS (`test_calc_engine` now runs the new 18).

- [ ] **Step 7: Commit**

```bash
git add lib/calc_engine/ test/test_calc_engine/
git commit -m "feat: calc_engine expression editing — digit/dot/op guards, caps, backspace"
```

---

### Task 2: CalcEngine smart paren() and percent()

**Files:**
- Modify: `lib/calc_engine/calc_engine.cpp` (add two function bodies)
- Modify: `test/test_calc_engine/test_main.cpp` (append tests)

**Interfaces:**
- Consumes: Task 1's `expr_` state, `lastNumberStart()`, `unclosed()`,
  `format()`, and the `isOp`/`isDigitCh` helpers.
- Produces: working `void paren()` and `void percent()` — Task 3's
  continuation tests and Task 4's UI mapping rely on them.

- [ ] **Step 1: Append the failing tests**

Add to `test/test_calc_engine/test_main.cpp` (before `main`):

```cpp
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
```

Add to `main`, before `return UNITY_END();`:

```cpp
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_calc_engine`
Expected: link FAILURE — undefined reference to `CalcEngine::paren()` and
`CalcEngine::percent()`.

- [ ] **Step 3: Implement paren() and percent()**

Add to `lib/calc_engine/calc_engine.cpp`, after `op()`:

```cpp
void CalcEngine::paren() {
  if (error_) return;
  if (lastWasResult_) {  // '(' after '=' starts a fresh expression
    expr_.clear();
    lastWasResult_ = false;
  }
  if (expr_.size() >= kMaxExprLen) return;
  const char last = expr_.empty() ? '\0' : expr_.back();
  if (expr_.empty() || isOp(last) || last == '(') {
    expr_ += '(';
    return;
  }
  if (unclosed(expr_) > 0 && (isDigitCh(last) || last == '.' || last == ')'))
    expr_ += ')';
  // Anything else (e.g. '(' straight after a digit): ignored — implicit
  // multiplication is not supported.
}

void CalcEngine::percent() {
  if (error_ || expr_.empty()) return;
  const char last = expr_.back();
  if (!isDigitCh(last) && last != '.') return;  // must end in a number
  const size_t numStart = lastNumberStart();
  const double v = std::strtod(expr_.c_str() + numStart, nullptr);
  const std::string repl = format(v / 100.0);
  if (numStart + repl.size() > kMaxExprLen) return;
  expr_.replace(numStart, expr_.size() - numStart, repl);
  lastWasResult_ = false;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_calc_engine`
Expected: 33 tests, all PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/calc_engine/ test/test_calc_engine/
git commit -m "feat: calc_engine smart paren key and expression-aware percent"
```

---

### Task 3: CalcEngine equals() — recursive-descent evaluation

**Files:**
- Modify: `lib/calc_engine/calc_engine.cpp` (parser + `equals()` body)
- Modify: `test/test_calc_engine/test_main.cpp` (append tests)

**Interfaces:**
- Consumes: Task 1's state/helpers; Task 2's `paren()`/`percent()` in tests.
- Produces: working `void equals()`; sets `lastWasResult_`, activating the
  after-`=` branches already written into `digit`/`dot`/`op`/`paren`/
  `backspace`/`percent`.

- [ ] **Step 1: Append the failing tests**

Add to `test/test_calc_engine/test_main.cpp` (before `main`). The helper
`type()` keeps sequence tests readable — add it right after the includes
section (below `tearDown`):

```cpp
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
```

Then the tests:

```cpp
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
```

Add to `main`, before `return UNITY_END();`:

```cpp
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_calc_engine`
Expected: link FAILURE — undefined reference to `CalcEngine::equals()`.

- [ ] **Step 3: Implement the parser and equals()**

In `lib/calc_engine/calc_engine.cpp`: add `#include <cmath>` to the include
block, add the parser inside the existing anonymous namespace (after
`isDigitCh`), and add the `equals()` body after `percent()`:

```cpp
// Recursive-descent evaluator (spec grammar):
//   expression := term (('+'|'-') term)*
//   term       := factor (('*'|'/') factor)*
//   factor     := '-'? ( number | '(' expression ')' )
// Input comes from the engine's own guards, so err is a backstop, not a
// user-facing validator; ÷0 is the one expected runtime failure.
struct Parser {
  const char* p;
  bool err = false;

  double parseExpression() {
    double v = parseTerm();
    while (!err && (*p == '+' || *p == '-')) {
      const char o = *p++;
      const double r = parseTerm();
      v = (o == '+') ? v + r : v - r;
    }
    return v;
  }

  double parseTerm() {
    double v = parseFactor();
    while (!err && (*p == '*' || *p == '/')) {
      const char o = *p++;
      const double r = parseFactor();
      if (err) break;
      if (o == '/' && r == 0.0) {  // graceful ÷0: error state, C recovers
        err = true;
        break;
      }
      v = (o == '*') ? v * r : v / r;
    }
    return v;
  }

  double parseFactor() {
    bool neg = false;
    if (*p == '-') {
      neg = true;
      ++p;
    }
    double v = 0.0;
    if (*p == '(') {
      ++p;
      v = parseExpression();
      if (!err && *p == ')') ++p;
      else err = true;
    } else if (isDigitCh(*p) || *p == '.') {
      char* end = nullptr;
      v = std::strtod(p, &end);
      p = end;
    } else {
      err = true;
    }
    return neg ? -v : v;
  }
};
```

```cpp
void CalcEngine::equals() {
  if (error_) return;
  std::string s = expr_;
  // Step 1 (spec): drop trailing operators and dangling opens.
  while (!s.empty() && (isOp(s.back()) || s.back() == '(')) s.pop_back();
  // Degenerate (no digits at all): no-op, expression stays as typed.
  if (s.find_first_of("0123456789") == std::string::npos) return;
  // Step 2: auto-close unclosed groups.
  for (int n = unclosed(s); n > 0; --n) s += ')';
  Parser parser{s.c_str()};
  const double v = parser.parseExpression();
  if (parser.err || *parser.p != '\0' || !std::isfinite(v)) {
    error_ = true;
    return;
  }
  expr_ = format(v);
  lastWasResult_ = true;
}
```

Note: very large results format in exponent form (e.g. `1e+24`). Continuing
with an operator still works — `strtod` consumes the exponent when the
expression is re-parsed. Backspacing into a malformed tail (e.g. `1e+`)
fails safe: `=` sets the error state and `C` recovers.

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_calc_engine`
Expected: 59 tests, all PASS.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all test dirs PASS.

- [ ] **Step 6: Commit**

```bash
git add lib/calc_engine/ test/test_calc_engine/
git commit -m "feat: calc_engine expression evaluation — precedence, parens, auto-close"
```

---

### Task 4: CalculatorApp UI — () key and ×/÷ display substitution

**Files:**
- Modify: `src/apps/calculator/CalculatorApp.cpp`

**Interfaces:**
- Consumes: `CalcEngine::paren()` (Task 2) and `display()` returning the
  full ASCII expression (Task 1).
- Produces: the flashable app for Task 5. No native tests (LVGL is
  device-only); the device build is the check.

- [ ] **Step 1: Swap the keypad key**

In `src/apps/calculator/CalculatorApp.cpp`, change the last row of
`kKeypadMap` from:

```cpp
    "+/-", "0", ".", "=", ""};
```

to:

```cpp
    "()", "0", ".", "=", ""};
```

- [ ] **Step 2: Remap the key handler**

In `handleKey`, replace:

```cpp
  else if (std::strcmp(t, "+/-") == 0) engine_.negate();
```

with:

```cpp
  else if (std::strcmp(t, "()") == 0) engine_.paren();
```

- [ ] **Step 3: Render ASCII operators as × and ÷**

Replace `refresh()`:

```cpp
void CalculatorApp::refresh() {
  if (displayLabel_ == nullptr) return;
  // Engine strings are ASCII; render '*' and '/' with the keypad's glyphs
  // (U+00D7 / U+00F7, both in montserrat_pt_14's Latin-1 range).
  const std::string raw = engine_.display();
  std::string out;
  out.reserve(raw.size() * 2);
  for (char c : raw) {
    if (c == '*') out += "\xC3\x97";       // ×
    else if (c == '/') out += "\xC3\xB7";  // ÷
    else out += c;
  }
  lv_label_set_text(displayLabel_, out.c_str());
}
```

Add `#include <string>` next to the existing `#include <cstring>`.

- [ ] **Step 4: Device build**

Run: `pio run -e cyd`
Expected: SUCCESS (native suite is unaffected — `test_build_src = false`).

- [ ] **Step 5: Commit**

```bash
git add src/apps/calculator/CalculatorApp.cpp
git commit -m "feat: calculator UI — smart () key, expression display with ×/÷ glyphs"
```

---

### Task 5: On-device verification (manual — needs the CYD)

**Files:** none (verification only).

**Interfaces:** consumes the flashed firmware from Task 4.

- [ ] **Step 1: Flash the device**

Run: `pio run -e cyd -t upload` (CYD enumerates as `/dev/ttyUSB0`; free the
serial port first if a monitor holds it).
Expected: upload completes; serial (115200) prints `danios: launcher up`.

- [ ] **Step 2: Keypad sanity**

Open Calculadora: row 5 now reads `()  0  .  =` (no `+/-`); `×`/`÷` render
as real glyphs, display shows a right-aligned `0`.

- [ ] **Step 3: Scripted key sequences (replaces the A1 plan's §8 table)**

Tap each sequence; the readout must match **exactly**:

| # | Key sequence | Expected display |
| --- | --- | --- |
| 1 | `( 1 0 + 2 0 ) ÷ 3 =` | `10` (the `()` key opens, then closes) |
| 2 | `2 + 3 × 4 =` | `14` (standard precedence — was 20 in A1) |
| 3 | `7 ÷ 2 =` | `3.5` |
| 4 | `( 2 + 4 =` | `6` (auto-close) |
| 5 | `5 ÷ 0 =` | `Erro`; then `7`, `+` still `Erro`; `C` → `0` |
| 6 | `1 2 3 ⌫ ⌫` | `1` |
| 7 | `- 5 × ( - 3 + 1 ) =` | `10` (unary minus, both positions) |
| 8 | `5 0 %` | `0.5` |
| 9 | `2 0 0 × 1 0 % =` | `20` |
| 10 | `. 1 + . 2 =` | `0.3` |
| 11 | `2 + 3 = × 2 =` | `10` (operator continues from result) |
| 12 | `2 + 3 = 7` | `7` (digit starts fresh) |
| 13 | `5 ( ( 9` | `59` (both `(` presses ignored after a digit; typing continues) |

While typing the expression the whole expression stays visible; type past
the label width and confirm the **newest** characters remain visible
(right-aligned label clips on the left). If the tail disappears instead,
file it — the label may need `lv_obj_set_style_text_align` reconsidered.

- [ ] **Step 4: Lifecycle checks (unchanged from A1)**

- Back arrow → launcher; re-opening shows a fresh `0`.
- Enter/leave five times fast — no crash, no visual leftovers.
- Screen sleep inside the app, tap to wake — waking tap presses no key.

---

## Definition of done

- [ ] `pio test -e native` — all test dirs green (59 calc_engine tests)
- [ ] `pio run -e cyd` — device build green
- [ ] Working expression calculator observed on hardware (Task 5 checklist)
- [ ] No SD paths, no NVS keys, no radio use introduced
- [ ] `lib/calc_engine/` has zero Arduino/LVGL includes
