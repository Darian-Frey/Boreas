// Wi-Fi station with capped-backoff reconnect (F-010).
//
// Polled from loop(); nothing here blocks. One Wi-Fi event callback
// (station disconnected) only records the event in atomics. An attempt
// fails as soon as the driver reports it over (IMP-004), with the explicit
// timeout (config::kWifiConnectTimeoutMs) as a backstop.
#pragma once

#include <cstdint>

#include "net/Backoff.h"

class WiFiMgr {
 public:
  enum class State : uint8_t { kOff, kConnecting, kConnected, kWaiting };

  // Starts the first association. Credentials must outlive the manager
  // (they come from secrets.h constants).
  void begin(const char* ssid, const char* password);

  // Advances the state machine. Call every loop() pass.
  void service(uint32_t now_ms);

  State state() const { return state_; }
  bool connected() const { return state_ == State::kConnected; }
  int8_t rssi() const;               // dBm; 0 when not connected
  uint32_t retry_in_ms(uint32_t now_ms) const;  // 0 unless waiting
  uint32_t reconnects() const { return reconnects_; }

  // Diagnostics for the health log. Reasons are ESP-IDF wifi_err_reason_t
  // codes; 0 means the attempt timed out with no driver report.
  uint32_t failed_attempts() const { return failed_attempts_; }
  uint8_t last_fail_reason() const { return last_fail_reason_; }
  uint8_t last_drop_reason() const { return last_drop_reason_; }
  int8_t last_rssi() const { return last_rssi_; }  // last reading while connected

 private:
  void start_attempt(uint32_t now_ms);
  void fail_attempt(uint32_t now_ms);

  const char* ssid_ = nullptr;
  const char* password_ = nullptr;
  State state_ = State::kOff;
  uint32_t state_since_ms_ = 0;
  uint32_t wait_ms_ = 0;
  uint32_t reconnects_ = 0;
  int last_status_ = -1;          // WiFi.status() last seen, for the change log
  uint32_t disconnects_at_start_ = 0;  // driver disconnect events when this attempt began
  bool first_attempt_ = true;     // the core retries its very first failure once by itself
  uint32_t failed_attempts_ = 0;
  uint8_t last_fail_reason_ = 0;
  uint8_t last_drop_reason_ = 0;
  int8_t last_rssi_ = 0;
  uint32_t last_rssi_ms_ = 0;
  Backoff backoff_{0, 0};
};
