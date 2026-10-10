// Wi-Fi station with capped-backoff reconnect (F-010).
//
// Polled from loop(): no Wi-Fi event callbacks, and nothing here blocks.
// Each association attempt has an explicit timeout
// (config::kWifiConnectTimeoutMs).
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

 private:
  void start_attempt(uint32_t now_ms);
  void fail_attempt(uint32_t now_ms);

  const char* ssid_ = nullptr;
  const char* password_ = nullptr;
  State state_ = State::kOff;
  uint32_t state_since_ms_ = 0;
  uint32_t wait_ms_ = 0;
  uint32_t reconnects_ = 0;
  Backoff backoff_{0, 0};
};
