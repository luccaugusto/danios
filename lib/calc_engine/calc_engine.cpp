#include "calc_engine.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>

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

double CalcEngine::entryValue() const {
  return std::strtod(entry_.c_str(), nullptr);  // "", "-", "." all parse as 0
}

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
