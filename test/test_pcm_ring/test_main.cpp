// Host-side tests for PcmRing (pio test -e native). Single-threaded here —
// what's under test is the index bookkeeping (wraparound, partial ops, the
// deferred-clear handshake), which is exactly what the two tasks share.
#include <unity.h>

#include "pcm_ring.h"

void setUp() {}
void tearDown() {}

static void fill(int16_t* b, size_t n, int16_t start) {
  for (size_t i = 0; i < n; ++i) b[i] = static_cast<int16_t>(start + i);
}

static void test_capacity_rounds_up_to_power_of_two() {
  PcmRing r(1000);
  TEST_ASSERT_EQUAL_UINT32(1024, (uint32_t)r.capacity());
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)r.available());
  TEST_ASSERT_EQUAL_UINT32(1024, (uint32_t)r.freeSpace());
}

static void test_write_read_roundtrip() {
  PcmRing r(8);
  int16_t in[6] = {1, -2, 3, -4, 5, -6};
  TEST_ASSERT_EQUAL_UINT32(6, (uint32_t)r.write(in, 6));
  TEST_ASSERT_EQUAL_UINT32(6, (uint32_t)r.available());
  int16_t out[6] = {};
  TEST_ASSERT_EQUAL_UINT32(6, (uint32_t)r.read(out, 6));
  TEST_ASSERT_EQUAL_INT16_ARRAY(in, out, 6);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)r.available());
}

static void test_partial_write_when_full() {
  PcmRing r(8);
  int16_t in[10];
  fill(in, 10, 0);
  TEST_ASSERT_EQUAL_UINT32(8, (uint32_t)r.write(in, 10));  // 2 don't fit
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)r.freeSpace());
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)r.write(in, 1));
}

static void test_read_from_empty_returns_zero() {
  PcmRing r(8);
  int16_t out[4];
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)r.read(out, 4));
}

static void test_wraparound_preserves_data() {
  PcmRing r(8);
  int16_t buf[6];
  int16_t out[6];
  fill(buf, 6, 100);
  r.write(buf, 6);
  r.read(out, 6);                 // indices now at 6 of an 8-slot buffer
  fill(buf, 6, 200);
  TEST_ASSERT_EQUAL_UINT32(6, (uint32_t)r.write(buf, 6));  // crosses the end
  TEST_ASSERT_EQUAL_UINT32(6, (uint32_t)r.read(out, 6));
  TEST_ASSERT_EQUAL_INT16_ARRAY(buf, out, 6);
}

static void test_request_clear_discards_only_older_samples() {
  PcmRing r(16);
  int16_t oldD[4];
  int16_t newD[4];
  fill(oldD, 4, 10);
  fill(newD, 4, 50);
  r.write(oldD, 4);
  r.requestClear();
  r.write(newD, 4);  // written after the clear -> must survive it
  int16_t out[8] = {};
  TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)r.read(out, 8));  // the old 4 are gone
  TEST_ASSERT_EQUAL_INT16_ARRAY(newD, out, 4);
}

static void test_clear_with_nothing_after_then_keeps_working() {
  PcmRing r(16);
  int16_t d[4];
  int16_t out[4];
  fill(d, 4, 1);
  r.write(d, 4);
  r.requestClear();
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)r.read(out, 4));  // all discarded
  r.write(d, 4);
  TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)r.read(out, 4));  // ring still healthy
  TEST_ASSERT_EQUAL_INT16_ARRAY(d, out, 4);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_capacity_rounds_up_to_power_of_two);
  RUN_TEST(test_write_read_roundtrip);
  RUN_TEST(test_partial_write_when_full);
  RUN_TEST(test_read_from_empty_returns_zero);
  RUN_TEST(test_wraparound_preserves_data);
  RUN_TEST(test_request_clear_discards_only_older_samples);
  RUN_TEST(test_clear_with_nothing_after_then_keeps_working);
  return UNITY_END();
}
