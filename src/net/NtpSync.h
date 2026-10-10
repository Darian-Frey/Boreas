// SNTP time sync and RTC write-back (D-007, D-015, F-009).
//
// Starts SNTP the first time Wi-Fi is up; lwIP then re-syncs on its own
// every config::kNtpIntervalMs. Each completed sync sets the system clock
// (UTC). service() then logs the RTC drift (AV-005) and writes the RTC
// with its NTP marker. The SNTP callback runs in the lwIP task and only
// sets a flag.
#pragma once

#include <cstdint>
#include <ctime>

class Rtc;

class NtpSync {
 public:
  explicit NtpSync(Rtc& rtc) : rtc_(rtc) {}

  // Registers the sync callback and the TZ rule. Call once at boot.
  void begin();

  // Call every loop() pass with the current Wi-Fi state.
  void service(uint32_t now_ms, bool wifi_connected);

  bool synced() const { return sync_count_ > 0; }
  uint32_t sync_count() const { return sync_count_; }
  // RTC minus NTP at the last sync, seconds; meaningful when synced().
  int32_t last_drift_s() const { return last_drift_s_; }
  bool last_rtc_write_ok() const { return last_rtc_write_ok_; }
  // True if Wi-Fi has been up for kNtpFirstSyncTimeoutMs with no sync yet.
  bool first_sync_overdue(uint32_t now_ms) const;

 private:
  Rtc& rtc_;
  bool started_ = false;
  uint32_t started_ms_ = 0;
  uint32_t sync_count_ = 0;
  int32_t last_drift_s_ = 0;
  bool last_rtc_write_ok_ = false;
};
