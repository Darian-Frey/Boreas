// Surface-pressure samples for the 3 h trend arrow (SPEC.md §Derived values).
//
// Keyed by the forecast's `current.time`, not by fetch count, because the
// fetch interval varies (15/30/60 min). The trend compares the latest
// sample with the stored sample closest to 3 h older (2.5-3.5 h); until
// one exists the trend is unknown. RAM only: lost on reboot (Phase 3 may
// move it to RTC memory).
#pragma once

#include <cstdint>
#include <ctime>

class PressureHistory {
 public:
  enum class Trend : uint8_t { kUnknown, kRising, kSteady, kFalling };

  static constexpr int kSlots = 16;              // 8 h at 30 min, 4 h at 15 min
  static constexpr int32_t kTargetAgeS = 3 * 3600;
  static constexpr int32_t kToleranceS = 30 * 60;
  static constexpr float kThresholdHpa = 1.0f;

  // Adds a sample. A sample with the same time as the latest replaces it
  // (Open-Meteo's current values are 15 min buckets). Older-than-latest
  // samples are ignored.
  void add(time_t t, float hpa) {
    if (count_ > 0) {
      const Sample& last = slots_[(head_ + kSlots - 1) % kSlots];
      if (t < last.t) return;
      if (t == last.t) {
        slots_[(head_ + kSlots - 1) % kSlots].hpa = hpa;
        return;
      }
    }
    slots_[head_] = {t, hpa};
    head_ = (head_ + 1) % kSlots;
    if (count_ < kSlots) ++count_;
  }

  Trend trend() const {
    if (count_ < 2) return Trend::kUnknown;
    const Sample& latest = slots_[(head_ + kSlots - 1) % kSlots];
    const Sample* best = nullptr;
    int32_t best_err = kToleranceS + 1;
    for (int i = 0; i < count_ - 1; ++i) {
      const Sample& s = slots_[(head_ + kSlots - 1 - 1 - i) % kSlots];
      const int32_t age = static_cast<int32_t>(latest.t - s.t);
      const int32_t err = age > kTargetAgeS ? age - kTargetAgeS : kTargetAgeS - age;
      if (err < best_err) {
        best_err = err;
        best = &s;
      }
    }
    if (!best) return Trend::kUnknown;
    const float d = latest.hpa - best->hpa;
    if (d >= kThresholdHpa) return Trend::kRising;
    if (d <= -kThresholdHpa) return Trend::kFalling;
    return Trend::kSteady;
  }

  int count() const { return count_; }

 private:
  struct Sample {
    time_t t;
    float hpa;
  };
  Sample slots_[kSlots] = {};
  int head_ = 0;   // next write position
  int count_ = 0;
};
