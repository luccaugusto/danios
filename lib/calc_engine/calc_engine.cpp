#include "calc_engine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr size_t kMaxNumberLen = 12;  // longest typeable number (formatted results from '=' or percent() may legitimately exceed this; downstream guards cope)
constexpr size_t kMaxExprLen = 48;    // whole-expression cap

bool isOp(char c) { return c == '+' || c == '-' || c == '*' || c == '/'; }
bool isDigitCh(char c) { return c >= '0' && c <= '9'; }

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
  if (numStart > 0) {  // exponent results ("1e+24") defeat lastNumberStart()
    const char prev = expr_[numStart - 1];
    const bool signAfterE = (prev == '+' || prev == '-') && numStart >= 2 &&
                            expr_[numStart - 2] == 'e';
    if (prev == 'e' || signAfterE) return;
  }
  const double v = std::strtod(expr_.c_str() + numStart, nullptr);
  const std::string repl = format(v / 100.0);
  if (numStart + repl.size() > kMaxExprLen) return;
  expr_.replace(numStart, expr_.size() - numStart, repl);
  lastWasResult_ = false;
}

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
