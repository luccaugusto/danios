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
