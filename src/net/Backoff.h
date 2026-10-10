// Capped exponential backoff: initial, 2x, 4x ... up to cap, then cap
// forever. Used for Wi-Fi reconnects (F-010) and, with SPEC.md §Retry
// parameters (1 min, cap 15 min), for forecast fetch retries (AV-003).
// Pure, so it's unit-tested on the host (test/test_net).
#pragma once

#include <cstdint>

class Backoff {
 public:
  constexpr Backoff(uint32_t initial_ms, uint32_t cap_ms)
      : initial_ms_(initial_ms), cap_ms_(cap_ms), next_ms_(initial_ms) {}

  // Delay to wait before the next attempt; advances the sequence.
  uint32_t next() {
    const uint32_t delay = next_ms_;
    next_ms_ = (next_ms_ >= cap_ms_ / 2) ? cap_ms_ : next_ms_ * 2;
    return delay;
  }

  // Back to the start of the sequence, after any success.
  void reset() { next_ms_ = initial_ms_; }

 private:
  uint32_t initial_ms_;
  uint32_t cap_ms_;
  uint32_t next_ms_;
};
