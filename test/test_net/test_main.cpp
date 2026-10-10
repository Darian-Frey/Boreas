// Host-native tests for src/net/Backoff (pio test -e native).

#include <unity.h>

#include "net/Backoff.h"

void setUp() {}
void tearDown() {}

void test_wifi_sequence_doubles_then_caps() {
  Backoff b(5000, 300000);
  const uint32_t expected[] = {5000, 10000, 20000, 40000, 80000, 160000, 300000, 300000};
  for (uint32_t e : expected) TEST_ASSERT_EQUAL_UINT32(e, b.next());
}

void test_fetch_retry_matches_spec() {
  // SPEC.md §Retry: 1, 2, 4, 8 min, then every 15 min.
  constexpr uint32_t kMin = 60000;
  Backoff b(1 * kMin, 15 * kMin);
  const uint32_t expected[] = {1 * kMin, 2 * kMin, 4 * kMin, 8 * kMin, 15 * kMin, 15 * kMin};
  for (uint32_t e : expected) TEST_ASSERT_EQUAL_UINT32(e, b.next());
}

void test_reset_restarts_sequence() {
  Backoff b(5000, 300000);
  b.next();
  b.next();
  b.next();
  b.reset();
  TEST_ASSERT_EQUAL_UINT32(5000, b.next());
  TEST_ASSERT_EQUAL_UINT32(10000, b.next());
}

void test_cap_not_power_of_two_multiple() {
  // Cap is reached exactly, never overshot, even when it isn't initial * 2^n.
  Backoff b(3000, 10000);
  TEST_ASSERT_EQUAL_UINT32(3000, b.next());
  TEST_ASSERT_EQUAL_UINT32(6000, b.next());
  TEST_ASSERT_EQUAL_UINT32(10000, b.next());
  TEST_ASSERT_EQUAL_UINT32(10000, b.next());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wifi_sequence_doubles_then_caps);
  RUN_TEST(test_fetch_retry_matches_spec);
  RUN_TEST(test_reset_restarts_sequence);
  RUN_TEST(test_cap_not_power_of_two_multiple);
  return UNITY_END();
}
