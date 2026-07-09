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
  void op(char o);           // '+', '-', '*', '/' — chains left-to-right
  void equals();
  std::string display() const;
  bool inError() const { return error_; }

 private:
  double entryValue() const;
  void applyPending();       // acc_ = acc_ <pendingOp_> entry; clears the op
  static std::string format(double v);

  double acc_ = 0.0;         // running result (left operand)
  char pendingOp_ = 0;       // '+', '-', '*', '/'; 0 = none
  std::string entry_;        // number being typed; empty = display shows acc_
  bool error_ = false;       // divide-by-zero / overflow; display() = "Erro"
};
