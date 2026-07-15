#include <unity.h>

#include <date_utils.h>

void setUp() {}
void tearDown() {}

void test_datekey_roundtrip() {
  LocalDate d{2026, 7, 3};
  TEST_ASSERT_EQUAL_UINT32(20260703u, dateKey(d));
  TEST_ASSERT_TRUE(fromDateKey(20260703u) == d);
}

void test_datekey_zero_sentinel() {
  LocalDate none{0, 0, 0};
  TEST_ASSERT_EQUAL_UINT32(0u, dateKey(none));
  TEST_ASSERT_TRUE(fromDateKey(0u) == none);
}

void test_days_between_same_day_is_zero() {
  LocalDate d{2026, 7, 3};
  TEST_ASSERT_EQUAL_INT32(0, daysBetween(d, d));
}

void test_days_between_simple() {
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2026, 7, 3}, {2026, 7, 4}));
  TEST_ASSERT_EQUAL_INT32(31, daysBetween({2026, 7, 1}, {2026, 8, 1}));
}

void test_days_between_across_year() {
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2025, 12, 31}, {2026, 1, 1}));
  TEST_ASSERT_EQUAL_INT32(365, daysBetween({2025, 1, 1}, {2026, 1, 1}));
}

void test_days_between_leap_year() {
  // 2024 is a leap year: Feb has 29 days.
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2024, 2, 28}, {2024, 2, 29}));
  TEST_ASSERT_EQUAL_INT32(2, daysBetween({2024, 2, 28}, {2024, 3, 1}));
  TEST_ASSERT_EQUAL_INT32(366, daysBetween({2024, 1, 1}, {2025, 1, 1}));
  // 2100 is NOT a leap year (divisible by 100, not by 400).
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2100, 2, 28}, {2100, 3, 1}));
}

void test_days_between_negative_span() {
  TEST_ASSERT_EQUAL_INT32(-1, daysBetween({2026, 7, 4}, {2026, 7, 3}));
  TEST_ASSERT_EQUAL_INT32(-365, daysBetween({2026, 1, 1}, {2025, 1, 1}));
}

void test_days_between_large_jump() {
  // Spec §4.5 clock-jump behavior relies on correct multi-year spans.
  TEST_ASSERT_EQUAL_INT32(9, daysBetween({2026, 6, 24}, {2026, 7, 3}));
  // 2000..2025 = 26*365 + 7 leap days = 9497; Jan 1 -> Jul 21 2026 = 201.
  TEST_ASSERT_EQUAL_INT32(9698, daysBetween({2000, 1, 1}, {2026, 7, 21}));
}

static void test_addDays_zero_and_unknown() {
  const LocalDate d{2026, 7, 6};
  TEST_ASSERT_TRUE(addDays(d, 0) == d);
  const LocalDate none{0, 0, 0};
  TEST_ASSERT_TRUE(addDays(none, 5) == none);  // sentinel passes through
}

static void test_addDays_forward_across_month_and_year() {
  TEST_ASSERT_TRUE(addDays({2026, 7, 31}, 1) == (LocalDate{2026, 8, 1}));
  TEST_ASSERT_TRUE(addDays({2026, 12, 31}, 1) == (LocalDate{2027, 1, 1}));
  TEST_ASSERT_TRUE(addDays({2026, 7, 6}, 30) == (LocalDate{2026, 8, 5}));
}

static void test_addDays_backward() {
  TEST_ASSERT_TRUE(addDays({2026, 8, 1}, -1) == (LocalDate{2026, 7, 31}));
  TEST_ASSERT_TRUE(addDays({2026, 1, 1}, -1) == (LocalDate{2025, 12, 31}));
}

static void test_addDays_leap_year() {
  TEST_ASSERT_TRUE(addDays({2024, 2, 28}, 1) == (LocalDate{2024, 2, 29}));
  TEST_ASSERT_TRUE(addDays({2024, 2, 28}, 2) == (LocalDate{2024, 3, 1}));
}

static void test_addDays_roundtrips_with_daysBetween() {
  const LocalDate a{2026, 7, 6};
  TEST_ASSERT_EQUAL_INT32(365, daysBetween(a, addDays(a, 365)));
  TEST_ASSERT_EQUAL_INT32(-90, daysBetween(a, addDays(a, -90)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_datekey_roundtrip);
  RUN_TEST(test_datekey_zero_sentinel);
  RUN_TEST(test_days_between_same_day_is_zero);
  RUN_TEST(test_days_between_simple);
  RUN_TEST(test_days_between_across_year);
  RUN_TEST(test_days_between_leap_year);
  RUN_TEST(test_days_between_negative_span);
  RUN_TEST(test_days_between_large_jump);
  RUN_TEST(test_addDays_zero_and_unknown);
  RUN_TEST(test_addDays_forward_across_month_and_year);
  RUN_TEST(test_addDays_backward);
  RUN_TEST(test_addDays_leap_year);
  RUN_TEST(test_addDays_roundtrips_with_daysBetween);
  return UNITY_END();
}
