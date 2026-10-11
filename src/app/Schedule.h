// Timing rules for app/ (SPEC.md §Scheduling, §Retry). Pure: no Arduino
// calls, clock values passed in, so every rule is tested on the host
// (test/test_app).
#pragma once

#include <cstdint>
#include <ctime>

#include "net/Backoff.h"

namespace schedule {

// 30 min normally, 15 min while an umbrella is advised, 60 min in night
// mode. Night wins over advisory: it exists to save power.
uint32_t fetch_interval_s(bool advisory, bool night);

// The first fetch slot at least half an interval after `now`. Slots are
// UTC multiples of the interval plus config::kFetchSlotOffsetS, which are
// also whole local quarter-hours because UK offsets are whole hours. The
// half-interval floor stops a fetch just before a slot from triggering
// another seconds later; the gap is therefore 0.5-1.5 intervals, always
// under the 2x stale threshold (F-007).
time_t next_slot(time_t now, uint32_t interval_s);

// Redraw when the minute changes (in either direction) or data changed (F-011).
inline bool redraw_due(int64_t minute_now, int64_t minute_last, bool dirty) {
  return dirty || minute_now != minute_last;
}

// When to fetch next: an aligned slot after success, capped backoff after
// failure (1, 2, 4, 8, then 15 min; AV-003).
class FetchScheduler {
 public:
  FetchScheduler();

  // True if a fetch should start now: never fetched, slot reached, or the
  // stored slot is implausibly far ahead (the clock was set backwards).
  bool due(time_t now) const;

  void on_success(time_t now, bool advisory, bool night);
  void on_failure(time_t now);

  time_t next() const { return next_; }        // 0 until the first result
  uint32_t failures() const { return failures_; }  // consecutive

 private:
  Backoff backoff_;
  time_t next_ = 0;
  uint32_t failures_ = 0;
};

}  // namespace schedule
