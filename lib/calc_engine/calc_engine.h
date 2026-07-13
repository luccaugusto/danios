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
