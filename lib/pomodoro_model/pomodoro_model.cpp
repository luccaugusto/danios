#include "pomodoro_model.h"

void PomoTimer::configure(const PomoConfig& c) {
  if (phase_ == PomoPhase::Idle) {
    cfg_ = c;
    // Floor zero-length phases to 1 minute to prevent infinite loops in catchUp().
    if (cfg_.work_min == 0) cfg_.work_min = 1;
    if (cfg_.break_min == 0) cfg_.break_min = 1;
  }
}

void PomoTimer::start(uint32_t now_ms) {
  if (phase_ != PomoPhase::Idle) return;
  phase_ = PomoPhase::Work;
  phaseStart_ = now_ms;
}

void PomoTimer::stop() { phase_ = PomoPhase::Idle; }

uint32_t PomoTimer::phaseLenMs(PomoPhase p) const {
  const uint16_t mins = (p == PomoPhase::Work) ? cfg_.work_min : cfg_.break_min;
  return static_cast<uint32_t>(mins) * 60000u;
}

// Unsigned subtraction makes `now - phaseStart_` wrap-safe as long as the
// timer is queried at least once per millis() wrap (~49.7 days).
void PomoTimer::catchUp(uint32_t now_ms) {
  if (phase_ == PomoPhase::Idle) return;
  while (now_ms - phaseStart_ >= phaseLenMs(phase_)) {
    phaseStart_ += phaseLenMs(phase_);
    phase_ = (phase_ == PomoPhase::Work) ? PomoPhase::Break : PomoPhase::Work;
  }
}

PomoPhase PomoTimer::phase(uint32_t now_ms) {
  catchUp(now_ms);
  return phase_;
}

uint32_t PomoTimer::remainingMs(uint32_t now_ms) {
  catchUp(now_ms);
  if (phase_ == PomoPhase::Idle) return 0;
  return phaseLenMs(phase_) - (now_ms - phaseStart_);
}
