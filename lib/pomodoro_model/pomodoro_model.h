// lib/pomodoro_model/pomodoro_model.h — pomodoro state machine (spec
// 2026-07-15-pomodoro-design). Pure C++17, no Arduino/LVGL: all time comes
// in as caller-supplied millis() values. Transitions resolve lazily in
// phase()/remainingMs() (catch-up loop), so the owner may stop querying for
// long stretches; the ~1 Hz badge poll in main.cpp bounds the gap well under
// the uint32 wrap.
#pragma once

#include <cstdint>

enum class PomoPhase : uint8_t { Idle, Work, Break };

struct PomoConfig {
  uint16_t work_min = 25;
  uint16_t break_min = 5;
};

class PomoTimer {
 public:
  void configure(const PomoConfig& c);  // ignored unless Idle; zero minutes floored to 1
  PomoConfig config() const { return cfg_; }

  void start(uint32_t now_ms);  // Idle -> Work; ignored while running
  void stop();                  // any -> Idle

  PomoPhase phase(uint32_t now_ms);       // resolves due transitions
  uint32_t remainingMs(uint32_t now_ms);  // 0 when Idle
  bool running() const { return phase_ != PomoPhase::Idle; }

 private:
  void catchUp(uint32_t now_ms);
  uint32_t phaseLenMs(PomoPhase p) const;

  PomoConfig cfg_{};
  PomoPhase phase_ = PomoPhase::Idle;
  uint32_t phaseStart_ = 0;  // millis at which the current phase began
};
