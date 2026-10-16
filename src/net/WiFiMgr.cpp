#include "net/WiFiMgr.h"

#include <WiFi.h>

#include <atomic>

#include "config.h"

namespace {

// Written from the Wi-Fi event task, read in loop(). Disconnects we cause
// ourselves (WiFi.disconnect() before each attempt) arrive as ASSOC_LEAVE
// and are ignored.
std::atomic<uint32_t> g_disconnects{0};
std::atomic<uint8_t> g_last_reason{0};

void on_sta_disconnected(arduino_event_id_t, arduino_event_info_t info) {
  const uint8_t reason = info.wifi_sta_disconnected.reason;
  if (reason == WIFI_REASON_ASSOC_LEAVE) return;
  g_last_reason.store(reason);
  g_disconnects.fetch_add(1);
}

}  // namespace

void WiFiMgr::begin(const char* ssid, const char* password) {
  ssid_ = ssid;
  password_ = password;
  backoff_ = Backoff(config::kWifiBackoffInitialMs, config::kWifiBackoffCapMs);

  WiFi.persistent(false);         // never write credentials to NVS
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config::kHostname);
  WiFi.setAutoReconnect(false);   // reconnect policy lives here, not in the driver
  WiFi.onEvent(on_sta_disconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  start_attempt(millis());
}

void WiFiMgr::start_attempt(uint32_t now_ms) {
  WiFi.disconnect();
  WiFi.begin(ssid_, password_);
  state_ = State::kConnecting;
  state_since_ms_ = now_ms;
  disconnects_at_start_ = g_disconnects.load();
}

void WiFiMgr::fail_attempt(uint32_t now_ms) {
  WiFi.disconnect();
  wait_ms_ = backoff_.next();
  state_ = State::kWaiting;
  state_since_ms_ = now_ms;
}

void WiFiMgr::service(uint32_t now_ms) {
  const wl_status_t status = WiFi.status();
  if (static_cast<int>(status) != last_status_) {
    Serial.printf("wifi status %d -> %d (state %d, %lu ms into it)\n", last_status_, static_cast<int>(status),
                  static_cast<int>(state_), static_cast<unsigned long>(now_ms - state_since_ms_));
    last_status_ = static_cast<int>(status);
  }
  const bool link_up = status == WL_CONNECTED;
  switch (state_) {
    case State::kOff:
      break;
    case State::kConnecting: {
      // The driver gives up on a refused association within ~2 s but leaves
      // WiFi.status() at WL_DISCONNECTED, the same as "still connecting", so
      // the disconnect event is the only early signal (IMP-004).
      const uint32_t ended = g_disconnects.load() - disconnects_at_start_;
      const uint32_t needed = first_attempt_ ? 2 : 1;
      if (link_up) {
        state_ = State::kConnected;
        state_since_ms_ = now_ms;
        backoff_.reset();
        first_attempt_ = false;
      } else if (ended >= needed) {
        Serial.printf("wifi attempt ended by the driver after %lu ms (reason %u)\n",
                      static_cast<unsigned long>(now_ms - state_since_ms_),
                      static_cast<unsigned>(g_last_reason.load()));
        first_attempt_ = false;
        fail_attempt(now_ms);
      } else if (now_ms - state_since_ms_ >= config::kWifiConnectTimeoutMs) {
        first_attempt_ = false;
        fail_attempt(now_ms);
      }
      break;
    }
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
