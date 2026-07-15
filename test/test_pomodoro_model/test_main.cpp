// Host-side tests for PomoTimer (pio test -e native).
//
// The timer is a pure state machine over caller-supplied millis() timestamps:
// Idle -> (start) -> Work <-> Break forever until stop(). Transitions resolve
// lazily in phase()/remainingMs() with a catch-up loop, so the timer is
// correct even when queried after several missed sections. All math is
// unsigned-wrap-safe (millis() wraps every ~49.7 days).
#include <unity.h>

#include "pomodoro_model.h"

void setUp() {}
void tearDown() {}

static constexpr uint32_t kMin = 60000u;  // ms per minute

static void test_starts_idle() {
  PomoTimer t;
  TEST_ASSERT_FALSE(t.running());
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Idle), static_cast<int>(t.phase(0)));
  TEST_ASSERT_EQUAL_UINT32(0, t.remainingMs(0));
}

static void test_default_config_25_5() {
  PomoTimer t;
  TEST_ASSERT_EQUAL_UINT16(25, t.config().work_min);
  TEST_ASSERT_EQUAL_UINT16(5, t.config().break_min);
}

static void test_start_enters_work_with_full_remaining() {
  PomoTimer t;
  t.start(1000);
  TEST_ASSERT_TRUE(t.running());
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work), static_cast<int>(t.phase(1000)));
  TEST_ASSERT_EQUAL_UINT32(25 * kMin, t.remainingMs(1000));
}

static void test_remaining_counts_down() {
  PomoTimer t;
  t.start(0);
  TEST_ASSERT_EQUAL_UINT32(25 * kMin - 1500, t.remainingMs(1500));
}

static void test_work_to_break_at_boundary() {
  PomoTimer t;
  t.start(0);
  // One ms before the boundary: still Work.
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work),
                    static_cast<int>(t.phase(25 * kMin - 1)));
  // At the boundary: Break, with the full break remaining.
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Break),
                    static_cast<int>(t.phase(25 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(5 * kMin, t.remainingMs(25 * kMin));
}

static void test_break_back_to_work() {
  PomoTimer t;
  t.start(0);
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work),
                    static_cast<int>(t.phase(30 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(25 * kMin, t.remainingMs(30 * kMin));
}

static void test_catch_up_over_multiple_missed_sections() {
  PomoTimer t;
  t.start(0);
  // 65 min = 2 full 30-min cycles + 5 min into the 3rd Work section.
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work),
                    static_cast<int>(t.phase(65 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(20 * kMin, t.remainingMs(65 * kMin));
}

static void test_stop_returns_to_idle() {
  PomoTimer t;
  t.start(0);
  t.stop();
  TEST_ASSERT_FALSE(t.running());
  TEST_ASSERT_EQUAL_UINT32(0, t.remainingMs(10 * kMin));
}

static void test_start_while_running_is_ignored() {
  PomoTimer t;
  t.start(0);
  t.start(10 * kMin);  // must not restart the section
  TEST_ASSERT_EQUAL_UINT32(15 * kMin, t.remainingMs(10 * kMin));
}

static void test_configure_applies_when_idle() {
  PomoTimer t;
  t.configure({50, 10});
  t.start(0);
  TEST_ASSERT_EQUAL_UINT32(50 * kMin, t.remainingMs(0));
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Break),
                    static_cast<int>(t.phase(50 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(10 * kMin, t.remainingMs(50 * kMin));
}

static void test_configure_while_running_is_ignored() {
  PomoTimer t;
  t.start(0);
  t.configure({50, 10});
  TEST_ASSERT_EQUAL_UINT16(25, t.config().work_min);
  TEST_ASSERT_EQUAL_UINT32(25 * kMin, t.remainingMs(0));
  // After stop, configure works again.
  t.stop();
  t.configure({50, 10});
  TEST_ASSERT_EQUAL_UINT16(50, t.config().work_min);
}

static void test_millis_wrap_is_safe() {
  PomoTimer t;
  // Start 10 min before the uint32 wrap; query 20 min later (past the wrap).
  const uint32_t start = 0xFFFFFFFFu - 10 * kMin;
  t.start(start);
  const uint32_t later = start + 20 * kMin;  // wraps
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work), static_cast<int>(t.phase(later)));
  TEST_ASSERT_EQUAL_UINT32(5 * kMin, t.remainingMs(later));
}

static void test_wrap_across_boundary() {
  PomoTimer t;
  const uint32_t start = 0xFFFFFFFFu - 10 * kMin;
  t.start(start);
  const uint32_t later = start + 27 * kMin;  // wraps, 2 min into Break
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Break), static_cast<int>(t.phase(later)));
  TEST_ASSERT_EQUAL_UINT32(3 * kMin, t.remainingMs(later));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_idle);
  RUN_TEST(test_default_config_25_5);
  RUN_TEST(test_start_enters_work_with_full_remaining);
  RUN_TEST(test_remaining_counts_down);
  RUN_TEST(test_work_to_break_at_boundary);
  RUN_TEST(test_break_back_to_work);
  RUN_TEST(test_catch_up_over_multiple_missed_sections);
  RUN_TEST(test_stop_returns_to_idle);
  RUN_TEST(test_start_while_running_is_ignored);
  RUN_TEST(test_configure_applies_when_idle);
  RUN_TEST(test_configure_while_running_is_ignored);
  RUN_TEST(test_millis_wrap_is_safe);
  RUN_TEST(test_wrap_across_boundary);
  return UNITY_END();
}
