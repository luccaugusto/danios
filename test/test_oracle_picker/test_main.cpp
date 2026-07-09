// Host-side tests for oracle_picker (pio test -e native).
//
// The picker is A2's native-TDD module: one wisdom index per civil day,
// stable all day, walking a per-cycle shuffled permutation so nothing
// repeats until the whole list has been shown (spec §4.4).
#include <unity.h>

#include <oracle_picker.h>

void setUp() {}
void tearDown() {}

static void test_count_zero_returns_zero() {
  // 0 is a defined, harmless answer; the app never renders with an empty
  // list (it shows the empty state instead).
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20260706u, 0));
}

static void test_count_one_always_zero() {
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20260101u, 1));
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20260706u, 1));
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20991231u, 1));
}

static void test_index_always_in_range() {
  const uint32_t kDates[] = {20260101u, 20260228u, 20260706u,
                             20261231u, 20270101u, 20991231u};
  const size_t kCounts[] = {2, 3, 7, 10, 137};
  for (uint32_t d : kDates)
    for (size_t c : kCounts)
      TEST_ASSERT_LESS_THAN_UINT(c, oraclePick(d, c));
}

static void test_same_inputs_same_index() {
  // "Stable all day": the pick is a pure function of (date, count).
  for (int i = 0; i < 5; ++i)
    TEST_ASSERT_EQUAL_UINT(oraclePick(20260706u, 42),
                           oraclePick(20260706u, 42));
}

static void test_unknown_date_sentinel_stays_in_range() {
  // dateKey 0 = "clock never synced" sentinel. The app handles it before
  // calling the picker, but the picker must not misbehave if it arrives.
  TEST_ASSERT_LESS_THAN_UINT(5u, oraclePick(0u, 5));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_count_zero_returns_zero);
  RUN_TEST(test_count_one_always_zero);
  RUN_TEST(test_index_always_in_range);
  RUN_TEST(test_same_inputs_same_index);
  RUN_TEST(test_unknown_date_sentinel_stays_in_range);
  return UNITY_END();
}
