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
