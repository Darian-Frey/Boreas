#include "app/Schedule.h"

#include "config.h"

namespace schedule {

namespace {

// Longest legitimate wait is 1.5 x the night interval; anything beyond this
// means the clock moved backwards under a stored slot.
constexpr time_t kMaxAheadS = 2 * static_cast<time_t>(config::kFetchNightIntervalS);

}  // namespace

uint32_t fetch_interval_s(bool advisory, bool night) {
  if (night) return config::kFetchNightIntervalS;
  if (advisory) return config::kFetchAdvisoryIntervalS;
  return config::kFetchIntervalS;
}

time_t next_slot(time_t now, uint32_t interval_s) {
  const time_t interval = static_cast<time_t>(interval_s);
  const time_t offset = static_cast<time_t>(config::kFetchSlotOffsetS);
  const time_t earliest = now + interval / 2;
  // Smallest offset + k*interval that is >= earliest.
  time_t k = (earliest - offset) / interval;
  time_t slot = offset + k * interval;
  if (slot < earliest) slot += interval;
  return slot;
}

FetchScheduler::FetchScheduler() : backoff_(config::kFetchRetryInitialS, config::kFetchRetryCapS) {}

bool FetchScheduler::due(time_t now) const {
  if (next_ == 0) return true;
  if (now >= next_) return true;
  return next_ - now > kMaxAheadS;
}

void FetchScheduler::on_success(time_t now, bool advisory, bool night) {
  backoff_.reset();
  failures_ = 0;
  next_ = next_slot(now, fetch_interval_s(advisory, night));
}

void FetchScheduler::on_failure(time_t now) {
  ++failures_;
  next_ = now + static_cast<time_t>(backoff_.next());
}

}  // namespace schedule
