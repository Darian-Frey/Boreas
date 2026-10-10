#include "net/NtpSync.h"

#include <Arduino.h>
#include <esp_sntp.h>

#include <atomic>

#include "config.h"
#include "sensors/Rtc.h"

namespace {

// Set from the lwIP task, consumed in loop().
std::atomic<bool> g_sync_pending{false};

void on_time_sync(struct timeval*) { g_sync_pending.store(true); }

}  // namespace

void NtpSync::begin() {
  sntp_set_time_sync_notification_cb(on_time_sync);
  sntp_set_sync_interval(config::kNtpIntervalMs);
  setenv("TZ", config::kTimezone, 1);
  tzset();
}

void NtpSync::service(uint32_t now_ms, bool wifi_connected) {
  if (!started_ && wifi_connected) {
    // Needs the network stack up; SNTP then keeps running across Wi-Fi
    // drops and retries on its own.
    configTzTime(config::kTimezone, config::kNtpServer1, config::kNtpServer2);
    started_ = true;
    started_ms_ = now_ms;
  }

  if (!g_sync_pending.exchange(false)) return;

  const time_t ntp_now = time(nullptr);
  const RtcReading before = rtc_.read();
  last_drift_s_ = before.valid ? static_cast<int32_t>(before.utc - ntp_now) : 0;
  last_rtc_write_ok_ = rtc_.set_from_ntp(ntp_now);
  ++sync_count_;
  Serial.printf("NTP sync %lu: utc=%lld, rtc was %s (drift %ld s), rtc write %s\n",
                static_cast<unsigned long>(sync_count_), static_cast<long long>(ntp_now),
                before.valid ? "valid" : "unset", static_cast<long>(last_drift_s_),
                last_rtc_write_ok_ ? "ok" : "FAILED");
}

bool NtpSync::first_sync_overdue(uint32_t now_ms) const {
  return started_ && sync_count_ == 0 && now_ms - started_ms_ >= config::kNtpFirstSyncTimeoutMs;
}
