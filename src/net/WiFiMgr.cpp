#include "net/WiFiMgr.h"

#include <WiFi.h>

#include "config.h"

void WiFiMgr::begin(const char* ssid, const char* password) {
  ssid_ = ssid;
  password_ = password;
  backoff_ = Backoff(config::kWifiBackoffInitialMs, config::kWifiBackoffCapMs);

  WiFi.persistent(false);         // never write credentials to NVS
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config::kHostname);
  WiFi.setAutoReconnect(false);   // reconnect policy lives here, not in the driver
  start_attempt(millis());
}

void WiFiMgr::start_attempt(uint32_t now_ms) {
  WiFi.disconnect();
  WiFi.begin(ssid_, password_);
  state_ = State::kConnecting;
  state_since_ms_ = now_ms;
}

void WiFiMgr::fail_attempt(uint32_t now_ms) {
  WiFi.disconnect();
  wait_ms_ = backoff_.next();
  state_ = State::kWaiting;
  state_since_ms_ = now_ms;
}

void WiFiMgr::service(uint32_t now_ms) {
  const bool link_up = WiFi.status() == WL_CONNECTED;
  switch (state_) {
    case State::kOff:
      break;
    case State::kConnecting:
      if (link_up) {
        state_ = State::kConnected;
        state_since_ms_ = now_ms;
        backoff_.reset();
      } else if (now_ms - state_since_ms_ >= config::kWifiConnectTimeoutMs) {
        fail_attempt(now_ms);
      }
      break;
    case State::kConnected:
      if (!link_up) {
        ++reconnects_;
        fail_attempt(now_ms);
      }
      break;
    case State::kWaiting:
      if (now_ms - state_since_ms_ >= wait_ms_) start_attempt(now_ms);
      break;
  }
}

int8_t WiFiMgr::rssi() const { return connected() ? static_cast<int8_t>(WiFi.RSSI()) : 0; }

uint32_t WiFiMgr::retry_in_ms(uint32_t now_ms) const {
  if (state_ != State::kWaiting) return 0;
  const uint32_t elapsed = now_ms - state_since_ms_;
  return elapsed >= wait_ms_ ? 0 : wait_ms_ - elapsed;
}
