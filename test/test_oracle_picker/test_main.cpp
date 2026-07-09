// Host-side tests for oracle_picker (pio test -e native).
//
// The picker is A2's native-TDD module: one wisdom index per civil day,
// stable all day, walking a per-cycle shuffled permutation so nothing
// repeats until the whole list has been shown (spec §4.4).
#include <unity.h>

#include <oracle_picker.h>
#include <date_utils.h>

#include <initializer_list>

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

// --- Task 2: the shuffle itself, tested on plain day serials -------------

static void test_cycle_zero_is_a_full_permutation() {
  // Serials 0..9 with count 10 are one aligned cycle: every index appears
  // exactly once — "no repeats until the whole list cycles".
  const size_t n = 10;
  bool seen[10] = {false};
  for (int32_t day = 0; day < 10; ++day) {
    const size_t idx = oraclePickAt(day, n);
    TEST_ASSERT_LESS_THAN_UINT(n, idx);
    TEST_ASSERT_FALSE_MESSAGE(seen[idx], "index repeated within a cycle");
    seen[idx] = true;
  }
}

static void test_later_cycle_is_also_a_full_permutation() {
  // An arbitrary aligned cycle far from epoch (serials 20660..20669 —
  // 2026-era days) must also cover every index exactly once.
  const size_t n = 10;
  const int32_t base = 2066 * 10;
  bool seen[10] = {false};
  for (int32_t day = base; day < base + 10; ++day) {
    const size_t idx = oraclePickAt(day, n);
    TEST_ASSERT_FALSE(seen[idx]);
    seen[idx] = true;
  }
}

static void test_cycles_have_different_orders() {
  // The whole point of the *date-seeded* shuffle: cycle k and cycle k+1
  // walk the list in different orders.
  const size_t n = 10;
  bool anyDifferent = false;
  for (int32_t pos = 0; pos < 10; ++pos)
    if (oraclePickAt(pos, n) != oraclePickAt(10 + pos, n)) anyDifferent = true;
  TEST_ASSERT_TRUE(anyDifferent);
}

static void test_order_is_shuffled_not_sequential() {
  // Not a bare "index = day % count" walk: somewhere in the cycle, the next
  // day is NOT simply previous+1 (mod count).
  const size_t n = 10;
  bool anyJump = false;
  for (int32_t day = 0; day < 9; ++day) {
    const size_t a = oraclePickAt(day, n);
    const size_t b = oraclePickAt(day + 1, n);
    if (b != (a + 1) % n) anyJump = true;
  }
  TEST_ASSERT_TRUE_MESSAGE(anyJump, "walk is sequential — not shuffled");
}

static void test_count_change_reshuffles_but_stays_valid() {
  // The maker's list grows/shrinks between boots: any count gives an
  // in-range, deterministic answer for the same day.
  const int32_t day = 20660;
  for (size_t n : {size_t{9}, size_t{10}, size_t{11}, size_t{137}}) {
    TEST_ASSERT_LESS_THAN_UINT(n, oraclePickAt(day, n));
    TEST_ASSERT_EQUAL_UINT(oraclePickAt(day, n), oraclePickAt(day, n));
  }
}

static void test_negative_serial_stays_in_range() {
  // Pre-1970 serials can't happen on-device, but floor div/mod keeps the
  // math total — no UB path.
  for (int32_t day = -25; day < 0; ++day)
    TEST_ASSERT_LESS_THAN_UINT(7u, oraclePickAt(day, 7));
}

static void test_pick_walks_serials_across_month_and_year_boundaries() {
  // oraclePick(dateKey) must land on the same walk position as the civil-day
  // serial — proven with date_utils across the two boundary kinds where a
  // naive dateKey walk breaks (YYYYMMDD jumps by 70/71 and ~8870).
  const LocalDate epoch{1970, 1, 1};
  const uint32_t kKeys[] = {20260731u, 20260801u, 20261231u, 20270101u};
  for (uint32_t key : kKeys) {
    const int32_t serial = daysBetween(epoch, fromDateKey(key));
    TEST_ASSERT_EQUAL_UINT(oraclePickAt(serial, 10), oraclePick(key, 10));
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_count_zero_returns_zero);
  RUN_TEST(test_count_one_always_zero);
  RUN_TEST(test_index_always_in_range);
  RUN_TEST(test_same_inputs_same_index);
  RUN_TEST(test_unknown_date_sentinel_stays_in_range);
  RUN_TEST(test_cycle_zero_is_a_full_permutation);
  RUN_TEST(test_later_cycle_is_also_a_full_permutation);
  RUN_TEST(test_cycles_have_different_orders);
  RUN_TEST(test_order_is_shuffled_not_sequential);
  RUN_TEST(test_count_change_reshuffles_but_stays_valid);
  RUN_TEST(test_negative_serial_stays_in_range);
  RUN_TEST(test_pick_walks_serials_across_month_and_year_boundaries);
  return UNITY_END();
}
