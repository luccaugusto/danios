# Calculator expression entry — design

**Date:** 2026-07-13
**Status:** approved
**Supersedes:** the immediate-execution semantics of the A1 calculator
(spec §8 scripted sequences and the `+/-` key).

## Goal

Replace the classic pocket-calculator (immediate-execution, left-to-right
chaining) behavior with expression entry: the user types a full expression
such as `(10+20)/3` on screen, and `=` evaluates it with standard math
precedence. Parentheses are supported via a single context-aware `()` key.

## Non-goals

- Expression history / memory keys (M+, MR).
- Scientific functions.
- Editing anywhere but the end of the expression (no cursor).

## Architecture

Two touched units, same boundary as A1:

1. **`lib/calc_engine/`** (pure C++17, zero Arduino/LVGL) — rewritten from an
   accumulator state machine into an expression editor + evaluator.
2. **`src/apps/calculator/CalculatorApp`** — keypad map swaps `+/-` for `()`;
   everything else (btnmatrix, single-line right-aligned display label)
   stays.

### CalcEngine public API

```cpp
class CalcEngine {
 public:
  void digit(char d);   // '0'..'9'
  void dot();
  void op(char o);      // '+', '-', '*', '/'
  void paren();         // smart ( / ) key
  void percent();
  void equals();
  void backspace();
  void clear();
  std::string display() const;
  bool inError() const;
};
```

`negate()` is removed — unary minus comes from the `-` key (valid at the
start of the expression and after `(`).

### Internal state

- `std::string expr_` — the expression as typed (ASCII: `0-9 . + - * / ( )`).
  The UI renders `*` and `/` as `×`/`÷` for display only.
- `bool lastWasResult_` — set after `=`; an operator continues from the
  result, a digit/dot/`(` starts a fresh expression.
- `bool error_` — `display()` returns `"Erro"`; only `clear()` exits.

## Editing semantics (input guards)

The expression is kept always-valid-so-far; invalid keys are ignored:

- **digit:** ignored right after `)` (an operator is required first).
  Leading-zero handling as in A1 (`0` then `5` types `5`). Each number is
  capped at 12 chars.
- **dot:** one per number; typing `.` at a number boundary inserts `0.`.
- **op:** a second operator in a row replaces the first. `-` is additionally
  allowed at the start of the expression and right after `(` (unary minus).
  `+`, `*`, `/` are ignored there.
- **paren():** inserts `(` at the start, after an operator, or after `(`;
  inserts `)` if there is an unclosed group and the previous char is a
  digit, `.` or `)`; otherwise ignored.
- **percent:** rewrites the number just typed to its value ÷ 100 (formatted
  like a typed number). Ignored if the expression doesn't end in a number.
- **backspace:** deletes the last character. No-op on empty / in error.
- **Whole-expression cap:** 48 chars; further input ignored. The display
  label clips on the left so the newest input stays visible.
- **After `=`:** operator keys continue from the result; digit/dot/`(`
  clear first and start fresh.

## Evaluation semantics

On `=`:

1. If the expression ends in an operator, drop it.
2. Auto-close unclosed parentheses (`(10+20` = evaluates `(10+20)`).
3. Evaluate with a recursive-descent parser, standard precedence:
   - `expression := term (('+'|'-') term)*`
   - `term := factor (('*'|'/') factor)*`
   - `factor := '-'? ( number | '(' expression ')' )`
4. Divide-by-zero or non-finite result → error state (`"Erro"`).
5. The formatted result becomes `expr_`; `lastWasResult_ = true`.

Formatting as in A1: `%.10g`, never `-0`.

Degenerate input: if after step 1 the expression contains no digits (empty,
`(`, `((-`), `=` is a no-op — the expression stays exactly as typed.

## Approach chosen

Recursive-descent parser (Approach A) over shunting-yard (B): same size,
one phase instead of two, grammar mirrors precedence directly, and error
handling is a simple bool. Incremental evaluation was rejected as
needless complexity.

## UI changes

- Keypad map row 5: `"+/-"` → `"()"`; `handleKey` maps `"()"` →
  `engine_.paren()` and drops the `negate()` branch.
- Display: unchanged single line. UI substitutes `*`→`×`, `/`→`÷` when
  rendering `display()` (engine stays ASCII; the label gets the pretty
  glyphs, consistent with the keypad).

## Behavior changes vs A1 (documented breakage)

- `2 + 3 × 4 =` now shows `14` (was `20`).
- `+/-` key removed; negative numbers are typed with `-` (e.g. `-5`,
  `(-3+1)`).
- While typing, the display shows the whole expression, not the current
  operand.

## Error handling

- `Erro` on divide-by-zero and non-finite results, exactly as A1; all keys
  except `C` ignored in error state.
- Malformed input is prevented by the input guards, not detected by the
  parser; the parser still fails safe (error state) on anything
  unexpected as a backstop.

## Testing

- Rewrite the native calc_engine test dirs to the new semantics. Coverage:
  precedence, nested parens, auto-close, unary minus, paren-key context
  rules, input guards (operator replacement, digit-after-`)`, dot rules),
  percent, backspace across tokens, 12-char number cap, 48-char expression
  cap, error states, `=`-continuation behavior, empty/degenerate
  expressions.
- New on-device scripted-sequence table (replaces spec §8's) including
  `( 1 0 + 2 0 ) ÷ 3 =` → `10`, `2 + 3 × 4 =` → `14` (precedence),
  `5 ÷ 0 =` → `Erro`, and an auto-close case such as `( 2 + 4 =` → `6`.
