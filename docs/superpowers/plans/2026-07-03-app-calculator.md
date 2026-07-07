# Calculator App (A1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Date:** 2026-07-06
**Spec:** [`docs/superpowers/specs/apps/calculator.md`](../specs/apps/calculator.md)
**Roadmap slot:** A1 (see [roadmap](2026-07-03-danios-roadmap.md) §1/§3 — names/paths there are authoritative)

**Goal:** Working four-function calculator on device — app id `"calc"` replaces the `calcStub` registration in `src/main.cpp`.

**Architecture:** All calculator behavior (digit entry, classic left-to-right operation chaining, divide-by-zero/overflow error states, display formatting) lives in a pure std-C++17 state machine in `lib/calc_engine/`, built TDD-first against `pio test -e native`. A thin LVGL wrapper `src/apps/calculator/CalculatorApp` builds an `lv_btnmatrix` keypad plus a result label, maps button text to engine calls, and renders `engine.display()`.

**Tech Stack:** C++17 (both envs), LVGL 8.4 (v8 API — `lv_btnmatrix`), PlatformIO (`cyd` device env, `native` host-test env), Unity test framework.

## Global Constraints

(Copied from roadmap §2 — every task inherits these.)

- **Board:** ESP32-2432S024C (CYD 2.4" capacitive), ESP32-WROOM-32, **no PSRAM**.
  520 KB SRAM total; budget carefully (LVGL buffers ~29 KB, LVGL heap 48 KB,
  WiFi ~50 KB or BT Classic ~64 KB — **never both**, MP3 decode ~30 KB).
- **Platform:** PlatformIO, `platform = espressif32@7.0.1`, `board = esp32dev`,
  `framework = arduino` (arduino-esp32 3.x). Partition scheme:
  `board_build.partitions = huge_app.csv` (no OTA — spec non-goal).
- **Display:** landscape-native 320×240 clone driven via
  `include/LGFX_ESP32_2432S024C.hpp` — **do not change panel/memory dims (320×240)
  or `offset_rotation` (0)**. All UI renders portrait 240×320 via
  `tft.setRotation(7)`, USB-C down. See `docs/DISPLAY.md` — read it before any
  display work.
- **LVGL:** `lvgl/lvgl@8.4.0` (v8 API — not v9). `lv_conf.h` lives in `include/`.
  UI code runs on the Arduino loop task only (LVGL is not thread-safe).
- **C++17** on both envs: `build_unflags = -std=gnu++11`,
  `build_flags = -std=gnu++17`.
- **Radio rule:** WiFi XOR Bluetooth, enforced only through `RadioManager`.
  Calculator needs no radio: `requiredRadio()` returns `RadioMode::None`.
- **TDD, native-first:** all pure logic lives in `lib/<module>/` with **zero
  Arduino/LVGL includes** (std C++ only) and is unit-tested with
  `pio test -e native` (Unity). Services/UI wrap the pure logic thinly.
- **Commits:** small, frequent, conventional (`feat:`, `test:`, `fix:`, `docs:`).
- **SD layout & NVS keys:** Calculator uses **no** SD paths and **no** NVS keys.

### Plan-specific facts (verified against the current tree)

- The Launcher gives `App::buildUI(lv_obj_t* parent)` a style-stripped container
  positioned below its own 32 px top bar: **240 wide × 288 tall**, origin (0,0)
  at the container's top-left (`src/core/Launcher.cpp:200-203`). The back arrow
  is the launcher's — the app must not add its own.
- The default font is the custom `montserrat_pt_14`
  (`src/assets/fonts/montserrat_pt_14.c`), which covers **full Latin-1
  (0xA0–0xFF)** — so `×` (U+00D7) and `÷` (U+00F7) render fine — plus the
  FontAwesome symbol set including `LV_SYMBOL_BACKSPACE` (U+F55A). The Unicode
  minus sign `−` (U+2212) is **NOT** in the font — always use ASCII `-`.
- PlatformIO adds each `lib/<module>/` to the include path: include the engine
  as `#include <calc_engine.h>` (same pattern as `<settings_store.h>`).
- Device UI language is Portuguese (see `src/apps/app_catalog.h`). The error
  display text is **`"Erro"`**.
- Launcher label/icon come from `catalog::kCalc` in `src/apps/app_catalog.h`
  (`"Calculadora"`, icon `nullptr` until the art file exists — drawing
  `S:/art/icons/calc.bin` is **out of scope** for this plan; the launcher
  renders its colored-letter fallback).
- Spec non-goal: **no** history, **no** memory keys (M+/MR/…). Do not add them.

## File Structure

| File | Task | Responsibility |
| --- | --- | --- |
| Create `lib/calc_engine/calc_engine.h` | 1–4 | Pure engine interface (grows one task at a time) |
| Create `lib/calc_engine/calc_engine.cpp` | 1–4 | Engine implementation — std C++17 only, zero Arduino/LVGL |
| Create `test/test_calc_engine/test_main.cpp` | 1–4 | Unity tests, one dir per lib module (repo convention) |
| Create `src/apps/calculator/CalculatorApp.h` | 5 | `App` subclass declaration |
| Create `src/apps/calculator/CalculatorApp.cpp` | 5 | LVGL keypad + label, key→engine mapping |
| Modify `src/main.cpp:31,116` | 5 | Replace `calcStub` with `CalculatorApp` |

### Engine design (locked in here, implemented across Tasks 1–4)

Classic pocket-calculator state machine. Three pieces of state plus an error
flag:

- `double acc_` — the running result (left operand). Starts `0.0`.
- `char pendingOp_` — `'+' '-' '*' '/'`, or `0` for none.
- `std::string entry_` — the number currently being typed, kept as text so the
  display shows exactly what was typed. **Empty means "not typing"** — the
  display then shows `acc_` formatted.
- `bool error_` — set by divide-by-zero or overflow. While set, every key
  except `C` is ignored and the display reads `"Erro"`.

Chaining (spec requirement, left-to-right, **not** precedence): pressing an
operator while another is pending evaluates the pending one first. So
`2 + 3 × 4 =` runs `2+3=5` at the `×` press, then `5×4=20` at `=`.

Formatting: `snprintf("%.10g")` — strips trailing zeros (`8/4=` shows `2`, not
`2.000000`), rounds double noise away (`0.1+0.2=` shows `0.3`), switches to
scientific notation for very large/small values so results always fit the
240 px row. `format(0.0)` is special-cased to `"0"` so `-0` never appears.

## Task Right-Sizing Overview

1. Engine: digit/dot entry, clear, display formatting
2. Engine: four operators, `=`, left-to-right chaining
3. Engine: error states — divide-by-zero and overflow
4. Engine: backspace, `+/-`, `%`
5. UI: `CalculatorApp` + registration in `main.cpp` + device build
6. On-device verification (manual, needs the CYD)

---

### Task 1: CalcEngine — digit entry, dot, clear, display

**Files:**
- Create: `lib/calc_engine/calc_engine.h`
- Create: `lib/calc_engine/calc_engine.cpp`
- Test: `test/test_calc_engine/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++ only).
- Produces: `class CalcEngine` with `void digit(char d)` (`'0'..'9'`),
  `void dot()`, `void clear()`, `std::string display() const`,
  `bool inError() const`. Tasks 2–4 add methods to this same class; Task 5's
  UI calls all of them.

- [ ] **Step 1: Write the failing tests**

Create `test/test_calc_engine/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_calc_engine`
Expected: **build error** — `calc_engine.h: No such file or directory`

- [ ] **Step 3: Write the minimal implementation**

Create `lib/calc_engine/calc_engine.h`:

```cpp
// lib/calc_engine/calc_engine.h — pure calculator state machine (roadmap §3,
// A1). std C++17 only, zero Arduino/LVGL includes. The UI wrapper feeds keys
// in and renders display(); all behavior is native-tested.
//
// Classic pocket-calculator semantics: operations chain left-to-right as
// entered (2 + 3 × 4 = evaluates as (2+3)×4 = 20 — no precedence).
#pragma once

#include <string>

class CalcEngine {
 public:
  void digit(char d);        // '0'..'9'
  void dot();                // decimal point (one per number)
  void clear();              // C — full reset; the only way out of error state
  std::string display() const;
  bool inError() const { return error_; }

 private:
  static std::string format(double v);

  double acc_ = 0.0;         // running result (left operand)
  char pendingOp_ = 0;       // '+', '-', '*', '/'; 0 = none
  std::string entry_;        // number being typed; empty = display shows acc_
  bool error_ = false;       // divide-by-zero / overflow; display() = "Erro"
};
```

Create `lib/calc_engine/calc_engine.cpp`:

```cpp
#include "calc_engine.h"

#include <cstdio>

namespace {
// Longest typeable number — keeps the entry inside the 240 px display row.
constexpr size_t kMaxEntryLen = 12;
}  // namespace

void CalcEngine::digit(char d) {
  if (error_) return;
  if (entry_.size() >= kMaxEntryLen) return;
  if (entry_ == "0") entry_.clear();        // "0" then "5" types "5", not "05"
  else if (entry_ == "-0") entry_ = "-";    // same, for a negated fresh entry
  entry_ += d;
}

void CalcEngine::dot() {
  if (error_) return;
  if (entry_.find('.') != std::string::npos) return;  // one dot per number
  if (entry_.empty() || entry_ == "-") entry_ += '0'; // "." types "0."
  if (entry_.size() >= kMaxEntryLen) return;
  entry_ += '.';
}

void CalcEngine::clear() {
  acc_ = 0.0;
  pendingOp_ = 0;
  entry_.clear();
  error_ = false;
}

std::string CalcEngine::display() const {
  if (error_) return "Erro";
  if (!entry_.empty()) return entry_;
  return format(acc_);
}

std::string CalcEngine::format(double v) {
  if (v == 0.0) return "0";  // also catches -0.0 → never display "-0"
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.10g", v);
  return buf;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_calc_engine`
Expected: `7 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Commit**

```bash
git add lib/calc_engine test/test_calc_engine
git commit -m "feat: calc_engine digit/dot entry, clear, display formatting"
```

---

### Task 2: CalcEngine — four operators, equals, left-to-right chaining

**Files:**
- Modify: `lib/calc_engine/calc_engine.h`
- Modify: `lib/calc_engine/calc_engine.cpp`
- Test: `test/test_calc_engine/test_main.cpp`

**Interfaces:**
- Consumes: Task 1's `CalcEngine` (`digit`, `display`, state fields).
- Produces: `void op(char o)` (`'+' '-' '*' '/'` — the UI maps `×`→`'*'`,
  `÷`→`'/'`), `void equals()`. Private helpers `applyPending()` and
  `entryValue()` that Task 3 modifies.

- [ ] **Step 1: Write the failing tests**

Add above `main()` in `test/test_calc_engine/test_main.cpp`:

```cpp
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
```

Add to `main()` before `return UNITY_END();`:

```cpp
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_calc_engine`
Expected: **build error** — `'class CalcEngine' has no member named 'op'`

- [ ] **Step 3: Write the implementation**

In `lib/calc_engine/calc_engine.h`, add to the `public:` section after `dot();`:

```cpp
  void op(char o);           // '+', '-', '*', '/' — chains left-to-right
  void equals();
```

and to the `private:` section before `static std::string format(double v);`:

```cpp
  double entryValue() const;
  void applyPending();       // acc_ = acc_ <pendingOp_> entry; clears the op
```

In `lib/calc_engine/calc_engine.cpp`, add `#include <cstdlib>` under
`#include <cstdio>`, then add after `dot()`:

```cpp
void CalcEngine::op(char o) {
  if (error_) return;
  if (!entry_.empty()) {
    if (pendingOp_ != 0) {
      applyPending();        // classic chaining: evaluate left-to-right
      if (error_) return;
    } else {
      acc_ = entryValue();
    }
    entry_.clear();
  }
  pendingOp_ = o;            // a second operator in a row replaces the first
}

void CalcEngine::equals() {
  if (error_) return;
  if (!entry_.empty()) {
    if (pendingOp_ != 0) applyPending();
    else acc_ = entryValue();
    entry_.clear();
  }
  pendingOp_ = 0;
}

double CalcEngine::entryValue() const {
  return std::strtod(entry_.c_str(), nullptr);  // "", "-", "." all parse as 0
}

void CalcEngine::applyPending() {
  const double rhs = entryValue();
  switch (pendingOp_) {
    case '+': acc_ += rhs; break;
    case '-': acc_ -= rhs; break;
    case '*': acc_ *= rhs; break;
    case '/': acc_ /= rhs; break;
  }
  pendingOp_ = 0;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_calc_engine`
Expected: `17 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Commit**

```bash
git add lib/calc_engine test/test_calc_engine
git commit -m "feat: calc_engine four ops with classic left-to-right chaining"
```

---

### Task 3: CalcEngine — divide-by-zero and overflow error states

**Files:**
- Modify: `lib/calc_engine/calc_engine.cpp` (only `applyPending()` changes)
- Test: `test/test_calc_engine/test_main.cpp`

**Interfaces:**
- Consumes: Task 2's `applyPending()`.
- Produces: behavior only — `error_` becomes reachable: `÷0` and non-finite
  results set it; `display()` then reads `"Erro"` (already implemented in
  Task 1); every key except `clear()` is ignored (guards already in place).

- [ ] **Step 1: Write the failing tests**

Add above `main()` in `test/test_calc_engine/test_main.cpp`:

```cpp
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
```

Add to `main()` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_divide_by_zero_shows_error);
  RUN_TEST(test_divide_by_zero_via_chaining);
  RUN_TEST(test_error_state_ignores_keys);
  RUN_TEST(test_clear_recovers_from_error);
  RUN_TEST(test_overflow_sets_error_not_inf);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_calc_engine`
Expected: FAIL — `test_divide_by_zero_shows_error` asserts
`Expected 'Erro' Was 'inf'` (and the other four new tests fail similarly).

- [ ] **Step 3: Write the implementation**

In `lib/calc_engine/calc_engine.cpp`, add `#include <cmath>` under
`#include <cstdio>`, and replace the whole `applyPending()` with:

```cpp
void CalcEngine::applyPending() {
  const double rhs = entryValue();
  if (pendingOp_ == '/' && rhs == 0.0) {  // graceful ÷0: error state, C recovers
    error_ = true;
    return;
  }
  switch (pendingOp_) {
    case '+': acc_ += rhs; break;
    case '-': acc_ -= rhs; break;
    case '*': acc_ *= rhs; break;
    case '/': acc_ /= rhs; break;
  }
  pendingOp_ = 0;
  if (!std::isfinite(acc_)) error_ = true;  // overflow → "Erro", never raw inf/nan
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_calc_engine`
Expected: `22 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Commit**

```bash
git add lib/calc_engine test/test_calc_engine
git commit -m "feat: calc_engine error states for divide-by-zero and overflow"
```

---

### Task 4: CalcEngine — backspace, sign toggle, percent

**Files:**
- Modify: `lib/calc_engine/calc_engine.h`
- Modify: `lib/calc_engine/calc_engine.cpp`
- Test: `test/test_calc_engine/test_main.cpp`

**Interfaces:**
- Consumes: Tasks 1–3 (`entry_`, `acc_`, `format`, `entryValue`).
- Produces: `void backspace()`, `void negate()`, `void percent()` — completes
  the engine API Task 5's UI consumes.

- [ ] **Step 1: Write the failing tests**

Add above `main()` in `test/test_calc_engine/test_main.cpp`:

```cpp
static void test_backspace_removes_last_char() {
  CalcEngine e;
  e.digit('1');
  e.digit('2');
  e.digit('3');
  e.backspace();
  TEST_ASSERT_EQUAL_STRING("12", e.display().c_str());
}

static void test_backspace_to_empty_shows_zero() {
  CalcEngine e;
  e.digit('5');
  e.backspace();
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
}

static void test_backspace_only_edits_typed_entry() {
  CalcEngine e;
  e.digit('2');
  e.op('+');
  e.digit('3');
  e.equals();      // result 5 is not a typed entry
  e.backspace();   // ignored
  TEST_ASSERT_EQUAL_STRING("5", e.display().c_str());
}

static void test_negate_toggles_entry_sign() {
  CalcEngine e;
  e.digit('4');
  e.digit('2');
  e.negate();
  TEST_ASSERT_EQUAL_STRING("-42", e.display().c_str());
  e.negate();
  TEST_ASSERT_EQUAL_STRING("42", e.display().c_str());
}

static void test_negate_applies_to_result() {
  CalcEngine e;
  e.digit('2');
  e.op('+');
  e.digit('3');
  e.equals();
  e.negate();
  TEST_ASSERT_EQUAL_STRING("-5", e.display().c_str());
}

static void test_negate_never_displays_minus_zero() {
  CalcEngine e;
  e.negate();  // negating the initial 0
  TEST_ASSERT_EQUAL_STRING("0", e.display().c_str());
}

static void test_negated_entry_used_in_arithmetic() {
  CalcEngine e;
  e.digit('5');
  e.op('+');
  e.digit('3');
  e.negate();  // rhs is -3
  e.equals();
  TEST_ASSERT_EQUAL_STRING("2", e.display().c_str());
}

static void test_percent_divides_entry_by_100() {
  CalcEngine e;
  e.digit('5');
  e.digit('0');
  e.percent();
  TEST_ASSERT_EQUAL_STRING("0.5", e.display().c_str());
}

static void test_percent_result_chains() {
  CalcEngine e;
  e.digit('2'); e.digit('0'); e.digit('0');
  e.op('*');
  e.digit('1'); e.digit('0');
  e.percent();  // rhs becomes 0.1
  e.equals();
  TEST_ASSERT_EQUAL_STRING("20", e.display().c_str());  // 200 × 10% = 20
}

static void test_percent_applies_to_result() {
  CalcEngine e;
  e.digit('2');
  e.op('+');
  e.digit('3');
  e.equals();  // 5
  e.percent();
  TEST_ASSERT_EQUAL_STRING("0.05", e.display().c_str());
}

static void test_editing_keys_ignored_in_error_state() {
  CalcEngine e;
  e.digit('5');
  e.op('/');
  e.digit('0');
  e.equals();
  e.backspace();
  e.negate();
  e.percent();
  TEST_ASSERT_EQUAL_STRING("Erro", e.display().c_str());
}
```

Add to `main()` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_backspace_removes_last_char);
  RUN_TEST(test_backspace_to_empty_shows_zero);
  RUN_TEST(test_backspace_only_edits_typed_entry);
  RUN_TEST(test_negate_toggles_entry_sign);
  RUN_TEST(test_negate_applies_to_result);
  RUN_TEST(test_negate_never_displays_minus_zero);
  RUN_TEST(test_negated_entry_used_in_arithmetic);
  RUN_TEST(test_percent_divides_entry_by_100);
  RUN_TEST(test_percent_result_chains);
  RUN_TEST(test_percent_applies_to_result);
  RUN_TEST(test_editing_keys_ignored_in_error_state);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_calc_engine`
Expected: **build error** — `'class CalcEngine' has no member named 'backspace'`

- [ ] **Step 3: Write the implementation**

In `lib/calc_engine/calc_engine.h`, add to the `public:` section after
`equals();`:

```cpp
  void backspace();          // ⌫ — edits the number being typed; else ignored
  void negate();             // +/- — toggles entry sign, or negates the result
  void percent();            // current value ÷ 100, becomes the entry
```

In `lib/calc_engine/calc_engine.cpp`, add after `equals()`:

```cpp
void CalcEngine::backspace() {
  if (error_ || entry_.empty()) return;
  entry_.pop_back();
  if (entry_ == "-") entry_.clear();
}

void CalcEngine::negate() {
  if (error_) return;
  if (entry_.empty()) {      // no entry being typed: negate the shown result
    acc_ = -acc_;
    return;
  }
  if (entry_[0] == '-') entry_.erase(0, 1);
  else entry_.insert(0, "-");
}

void CalcEngine::percent() {
  if (error_) return;
  const double v = entry_.empty() ? acc_ : entryValue();
  entry_ = format(v / 100.0);  // becomes the entry, so it chains like typed input
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_calc_engine`
Expected: `33 Tests 0 Failures 0 Ignored` — PASSED

Also run the full suite to confirm nothing else broke:

Run: `pio test -e native`
Expected: all five test dirs PASSED (`test_calc_engine`, `test_fs_names`,
`test_launcher_model`, `test_press_debounce`, `test_settings_store`).

- [ ] **Step 5: Commit**

```bash
git add lib/calc_engine test/test_calc_engine
git commit -m "feat: calc_engine backspace, sign toggle, percent"
```

---

### Task 5: CalculatorApp UI wrapper + registration

**Files:**
- Create: `src/apps/calculator/CalculatorApp.h`
- Create: `src/apps/calculator/CalculatorApp.cpp`
- Modify: `src/main.cpp:7-9` (include), `src/main.cpp:31` (instance),
  `src/main.cpp:116` (registration)

**Interfaces:**
- Consumes: the complete `CalcEngine` (Tasks 1–4); `App` from
  `src/core/App.h`; `catalog::kCalc` from `src/apps/app_catalog.h`;
  `LV_SYMBOL_BACKSPACE`.
- Produces: `class CalculatorApp : public App` with a default constructor —
  `main.cpp` creates one static instance and passes it to
  `launcher.registerApp(...)` in the third grid slot.

No native test — this is thin LVGL glue; the device build is the check and
Task 6 verifies behavior on hardware.

- [ ] **Step 1: Write the header**

Create `src/apps/calculator/CalculatorApp.h`:

```cpp
// src/apps/calculator/CalculatorApp.h — Calculator app (A1, spec §4.3).
// Thin LVGL wrapper: btnmatrix keypad + result label; all behavior lives in
// lib/calc_engine (native-tested). No radio, no SD, no NVS.
#pragma once

#include <calc_engine.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class CalculatorApp : public App {
 public:
  const char* id() const override { return "calc"; }
  const char* title() const override { return catalog::kCalc.title; }
  const char* iconPath() const override { return catalog::kCalc.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override { engine_.clear(); }  // fresh calc on every open
  void buildUI(lv_obj_t* parent) override;
  void onExit() override { displayLabel_ = nullptr; }  // launcher deletes widgets

 private:
  static void keyPressed(lv_event_t* e);
  void handleKey(const char* txt);
  void refresh();

  CalcEngine engine_;
  lv_obj_t* displayLabel_ = nullptr;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/apps/calculator/CalculatorApp.cpp`:

```cpp
#include "apps/calculator/CalculatorApp.h"

#include <cstring>

namespace {

// 4 columns × 5 rows. × (U+00D7) and ÷ (U+00F7) exist in montserrat_pt_14
// (full Latin-1 range); the backspace glyph comes from its FontAwesome range.
// Minus must stay ASCII '-' — U+2212 is not in the font.
const char* kKeypadMap[] = {
    "C", LV_SYMBOL_BACKSPACE, "%", "÷", "\n",
    "7", "8", "9", "×", "\n",
    "4", "5", "6", "-", "\n",
    "1", "2", "3", "+", "\n",
    "+/-", "0", ".", "=", ""};

constexpr lv_coord_t kDisplayH = 56;  // readout strip; keypad fills the rest

}  // namespace

void CalculatorApp::buildUI(lv_obj_t* parent) {
  // Parent is the launcher's style-stripped 240×288 container below the top
  // bar (back arrow is the launcher's — none here).
  displayLabel_ = lv_label_create(parent);
  lv_label_set_long_mode(displayLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(displayLabel_, 240 - 16);
  lv_obj_set_style_text_align(displayLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(displayLabel_, LV_ALIGN_TOP_MID, 0, (kDisplayH - 16) / 2);

  lv_obj_t* pad = lv_btnmatrix_create(parent);
  lv_btnmatrix_set_map(pad, kKeypadMap);
  lv_obj_set_pos(pad, 0, kDisplayH);
  lv_obj_set_size(pad, 240, 288 - kDisplayH);  // 5 rows ≈ 46 px — good targets
  lv_obj_add_event_cb(pad, keyPressed, LV_EVENT_VALUE_CHANGED, this);

  refresh();
}

void CalculatorApp::keyPressed(lv_event_t* e) {
  auto* self = static_cast<CalculatorApp*>(lv_event_get_user_data(e));
  lv_obj_t* pad = lv_event_get_target(e);
  const uint16_t id = lv_btnmatrix_get_selected_btn(pad);
  if (id == LV_BTNMATRIX_BTN_NONE) return;
  self->handleKey(lv_btnmatrix_get_btn_text(pad, id));
}

void CalculatorApp::handleKey(const char* t) {
  if (t[0] >= '0' && t[0] <= '9' && t[1] == '\0') engine_.digit(t[0]);
  else if (std::strcmp(t, ".") == 0) engine_.dot();
  else if (std::strcmp(t, "+") == 0) engine_.op('+');
  else if (std::strcmp(t, "-") == 0) engine_.op('-');
  else if (std::strcmp(t, "×") == 0) engine_.op('*');
  else if (std::strcmp(t, "÷") == 0) engine_.op('/');
  else if (std::strcmp(t, "=") == 0) engine_.equals();
  else if (std::strcmp(t, "C") == 0) engine_.clear();
  else if (std::strcmp(t, LV_SYMBOL_BACKSPACE) == 0) engine_.backspace();
  else if (std::strcmp(t, "+/-") == 0) engine_.negate();
  else if (std::strcmp(t, "%") == 0) engine_.percent();
  refresh();
}

void CalculatorApp::refresh() {
  if (displayLabel_ == nullptr) return;
  lv_label_set_text(displayLabel_, engine_.display().c_str());
}
```

- [ ] **Step 3: Replace the stub registration in main.cpp**

In `src/main.cpp`, add the include after `#include "apps/app_catalog.h"`
(line 8):

```cpp
#include "apps/calculator/CalculatorApp.h"
```

Replace line 31 (`static StubApp calcStub("calc", catalog::kCalc);`) with:

```cpp
static CalculatorApp calculatorApp;
```

Replace line 116 (`launcher.registerApp(&calcStub);`) with — **same position,
third registration, so the grid order is unchanged**:

```cpp
launcher.registerApp(&calculatorApp);
```

Do not touch the `setAppEnabled` block — Calculator never depends on the SD
card (the comment at `src/main.cpp:123-125` already says so).

- [ ] **Step 4: Build for the device and re-run native tests**

Run: `pio run -e cyd`
Expected: `SUCCESS` (RAM/Flash usage printed; no warnings from new files)

Run: `pio test -e native`
Expected: all five test dirs PASSED

- [ ] **Step 5: Commit**

```bash
git add src/apps/calculator src/main.cpp
git commit -m "feat: Calculator app — btnmatrix keypad wired to calc_engine"
```

---

### Task 6: On-device verification (manual — needs the CYD)

**Files:** none (verification only).

**Interfaces:** consumes the flashed firmware from Task 5.

- [ ] **Step 1: Flash the device**

Run: `pio run -e cyd -t upload` (CYD enumerates as `/dev/ttyUSB0`)
Expected: upload completes; serial (115200) prints `danios: launcher up`

- [ ] **Step 2: Launcher integration checks**

- Grid slot 3 shows the colored-letter fallback icon labeled **Calculadora**
  (no icon art yet — `catalog::kCalc.icon` is nullptr by design).
- Tap it: app opens, top bar shows the launcher's back arrow + "Calculadora",
  a right-aligned `0` readout, and the 4×5 keypad with `×`/`÷` rendered as
  real glyphs (not missing-glyph boxes).
- Status bar radio glyph stays "none" (calculator requests no radio).

- [ ] **Step 3: Scripted key sequences (spec §8)**

Tap each sequence; the readout must match **exactly**:

| # | Key sequence | Expected display |
| --- | --- | --- |
| 1 | `2 + 3 × 4 =` | `20` (chaining, not precedence) |
| 2 | `7 ÷ 2 =` | `3.5` |
| 3 | `8 ÷ 4 =` | `2` (no trailing zeros) |
| 4 | `5 ÷ 0 =` | `Erro` |
| 5 | (after #4) `7`, `+` | still `Erro` (keys ignored) |
| 6 | (after #5) `C` | `0`, then `1 + 1 =` → `2` |
| 7 | `1 2 3 ⌫` | `12` |
| 8 | `4 2 +/-` | `-42` |
| 9 | `5 0 %` | `0.5` |
| 10 | `2 0 0 × 1 0 % =` | `20` |
| 11 | `. 1 + . 2 =` | `0.3` |
| 12 | thirteen `9`s | `999999999999` (entry capped at 12) |

- [ ] **Step 4: Lifecycle checks**

- Back arrow returns to the launcher; re-opening Calculator shows a fresh `0`
  (state intentionally resets in `onEnter`).
- Enter/leave the app five times fast — no crash, no visual leftovers
  (launcher cleans the container; `onExit` drops the dangling label pointer).
- Let the screen sleep inside the calculator (default 60 s), tap to wake —
  the waking tap must **not** press a calculator key (F3's tap shield).

---

## Definition of done

- [ ] `pio test -e native` — all five test dirs green (33 calc_engine tests)
- [ ] `pio run -e cyd` — device build green
- [ ] Roadmap §1 E2E outcome observed on hardware: working four-function
      calculator on device (Task 6 checklist complete)
- [ ] No SD paths, no NVS keys, no radio use introduced
- [ ] `lib/calc_engine/` has zero Arduino/LVGL includes
