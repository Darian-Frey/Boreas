// Host-native tests for the timing rules in src/app/Schedule (SPEC.md
// §Scheduling, §Retry; F-008, F-011; AV-003). pio test -e native.

#include <unity.h>

#include <initializer_list>

#include "app/Schedule.h"
#include "config.h"

void setUp() {}
void tearDown() {}

// 2026-10-06, UTC (values from `date -u -d ... +%s`). Slots sit 7 min 15 s
// past the interval (IMP-005): :07:15 / :37:15 at 30 min.
constexpr time_t k120742 = 1791288462;
constexpr time_t k122215 = 1791289335;
constexpr time_t k122300 = 1791289380;
constexpr time_t k123650 = 1791290210;
constexpr time_t k123715 = 1791290235;
constexpr time_t k125215 = 1791291135;
constexpr time_t k130715 = 1791292035;

// ---------------------------------------------------------------- Intervals

void test_interval_normal_advisory_night() {
  TEST_ASSERT_EQUAL_UINT32(1800, schedule::fetch_interval_s(false, false));
  TEST_ASSERT_EQUAL_UINT32(900, schedule::fetch_interval_s(true, false));   // UMBRELLA ADVISED
  TEST_ASSERT_EQUAL_UINT32(3600, schedule::fetch_interval_s(false, true));  // night mode (F-014)
  TEST_ASSERT_EQUAL_UINT32(3600, schedule::fetch_interval_s(true, true));   // night wins: it exists to save power
}

// ---------------------------------------------------------------- Alignment

void test_next_slot_aligned_with_offset() {
  TEST_ASSERT_EQUAL_INT64(k123715, schedule::next_slot(k120742, 1800));  // :07:15 / :37:15
  TEST_ASSERT_EQUAL_INT64(k122215, schedule::next_slot(k120742, 900));   // :07:15 / :22:15 / ...
  TEST_ASSERT_EQUAL_INT64(k130715, schedule::next_slot(k120742, 3600));  // hh:07:15
}

void test_next_slot_is_strictly_later_from_a_slot() {
  // A fetch that ran exactly on its slot schedules the following one.
  TEST_ASSERT_EQUAL_INT64(k130715, schedule::next_slot(k123715, 1800));
  TEST_ASSERT_EQUAL_INT64(k125215, schedule::next_slot(k123715, 900));
}

void test_next_slot_skips_a_slot_closer_than_half_an_interval() {
  // Boot fetch at 12:36:50 must not fetch again at 12:37:15, 25 s later.
  TEST_ASSERT_EQUAL_INT64(k130715, schedule::next_slot(k123650, 1800));
  // 12:23:00 -> 12:37:15 is 14 min 15 s, under 15 min: skip to 13:07:15.
  TEST_ASSERT_EQUAL_INT64(k130715, schedule::next_slot(k122300, 1800));
}

void test_gap_never_exceeds_one_and_a_half_intervals() {
  // Stale is 2x the interval (F-007), so normal scheduling never trips it.
  for (uint32_t interval : {900u, 1800u, 3600u}) {
    for (time_t now = k120742; now < k120742 + 2 * 3600; now += 7) {
      const time_t next = schedule::next_slot(now, interval);
      TEST_ASSERT_TRUE(next - now >= static_cast<time_t>(interval / 2));
      TEST_ASSERT_TRUE(next - now <= static_cast<time_t>(interval + interval / 2));
      TEST_ASSERT_EQUAL_INT64(0, (next - config::kFetchSlotOffsetS) % interval);  // on a slot
    }
  }
}

void test_no_slot_near_the_top_of_the_hour() {
  // IMP-005: Open-Meteo returned 503s at hh:00 in the soak, so no slot may
  // fall in the first 5 minutes of an hour, at any interval.
  for (uint32_t interval : {900u, 1800u, 3600u}) {
    for (time_t now = k120742; now < k120742 + 2 * 3600; now += 7) {
      const time_t into_hour = schedule::next_slot(now, interval) % 3600;
      TEST_ASSERT_TRUE(into_hour >= 5 * 60);
    }
  }
}

// ---------------------------------------------------------------- FetchScheduler

void test_first_fetch_is_due_immediately() {
  schedule::FetchScheduler f;
  TEST_ASSERT_TRUE(f.due(k120742));
}

void test_success_schedules_aligned_slot() {
  schedule::FetchScheduler f;
  f.on_success(k120742, false, false);
  TEST_ASSERT_EQUAL_INT64(k123715, f.next());
  TEST_ASSERT_FALSE(f.due(k123715 - 1));
  TEST_ASSERT_TRUE(f.due(k123715));
}

void test_advisory_shortens_next_fetch() {
  schedule::FetchScheduler f;
  f.on_success(k120742, true, false);
  TEST_ASSERT_EQUAL_INT64(k122215, f.next());
}

void test_failures_back_off_per_spec() {
  // SPEC.md §Retry: 1, 2, 4, 8 min, then every 15 min (AV-003).
  schedule::FetchScheduler f;
  time_t now = k120742;
  const time_t expected[] = {60, 120, 240, 480, 900, 900, 900};
  for (time_t gap : expected) {
    f.on_failure(now);
    TEST_ASSERT_EQUAL_INT64(now + gap, f.next());
    now = f.next();
  }
  TEST_ASSERT_EQUAL_UINT32(7, f.failures());
}

void test_success_resets_backoff() {
  schedule::FetchScheduler f;
  f.on_failure(k120742);
  f.on_failure(k120742 + 60);
  f.on_success(k120742 + 180, false, false);
  TEST_ASSERT_EQUAL_UINT32(0, f.failures());
  TEST_ASSERT_EQUAL_INT64(k123715, f.next());
  f.on_failure(k123715);
  TEST_ASSERT_EQUAL_INT64(k123715 + 60, f.next());  // back to 1 min
}

void test_clock_jumping_backwards_does_not_stall_fetches() {
  // NTP corrects the clock back an hour; the old "next" is now 1.5 h away.
  schedule::FetchScheduler f;
  f.on_success(k120742, false, true);  // next 13:07:15
  TEST_ASSERT_FALSE(f.due(k120742 - 600));  // within a plausible interval: wait
  TEST_ASSERT_TRUE(f.due(k120742 - 3 * 3600));  // implausibly far ahead: fetch now
}

// ---------------------------------------------------------------- Redraw

void test_redraw_on_the_minute_and_on_change() {
  const int64_t m = k120742 / 60;
  TEST_ASSERT_TRUE(schedule::redraw_due(m, m - 1, false));  // new minute
  TEST_ASSERT_FALSE(schedule::redraw_due(m, m, false));     // same minute, nothing changed
  TEST_ASSERT_TRUE(schedule::redraw_due(m, m, true));       // data changed
  TEST_ASSERT_TRUE(schedule::redraw_due(m - 5, m, false));  // clock went backwards: redraw
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_interval_normal_advisory_night);
  RUN_TEST(test_next_slot_aligned_with_offset);
  RUN_TEST(test_next_slot_is_strictly_later_from_a_slot);
  RUN_TEST(test_next_slot_skips_a_slot_closer_than_half_an_interval);
  RUN_TEST(test_gap_never_exceeds_one_and_a_half_intervals);
  RUN_TEST(test_no_slot_near_the_top_of_the_hour);
  RUN_TEST(test_first_fetch_is_due_immediately);
  RUN_TEST(test_success_schedules_aligned_slot);
  RUN_TEST(test_advisory_shortens_next_fetch);
  RUN_TEST(test_failures_back_off_per_spec);
  RUN_TEST(test_success_resets_backoff);
  RUN_TEST(test_clock_jumping_backwards_does_not_stall_fetches);
  RUN_TEST(test_redraw_on_the_minute_and_on_change);
  return UNITY_END();
}
