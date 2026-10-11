#include "app/App.h"

#include <Arduino.h>
#include <sys/time.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h: copy include/secrets.example.h and add Wi-Fi credentials (D-010)"
#endif
#if __has_include("locations.h")
#include "locations.h"
#else
#error "Missing include/locations.h: copy include/locations.example.h and set your location"
#endif

#include "config.h"
#include "data/Derived.h"
#include "display/MemCanvas.h"
#include "pins.h"
#include "ui/Footer.h"
#include "ui/Frame.h"
#include "ui/Indoor.h"
#include "ui/Outdoor.h"
#include "ui/RainMeter.h"
#include "ui/Readouts.h"
#include "ui/TestCard.h"
#include "ui/Trace.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
// Night mode (F-014, Phase 3) is not implemented yet; the schedule already
// takes the flag.
constexpr bool kNightMode = false;

const char* trend_name(PressureHistory::Trend t) {
  static const char* const kNames[] = {"?", "UP", "STEADY", "DOWN"};
  return kNames[static_cast<int>(t)];
}

}  // namespace

void App::begin() {
  Serial.begin(kSerialBaud);
  // An unread USB CDC port must drop output, not stall loop() (AV-011).
  Serial.setTxTimeoutMs(0);
  Serial.println("\nBoreas MS-42");

  Wire.begin(pins::kI2cSda, pins::kI2cScl);
  battery_.begin();
  Serial.printf("SHTC3 %s\n", shtc3_.begin() ? "OK" : "NOT FOUND");

  // D-007: show a correct clock before Wi-Fi, but only from a trusted RTC (D-015).
  ntp_.begin();
  const RtcReading boot_rtc = rtc_.read();
  if (boot_rtc.valid) {
    struct timeval tv = {boot_rtc.utc, 0};
    settimeofday(&tv, nullptr);
    clock_valid_ = true;
  }
  Serial.printf("RTC at boot: %s\n", boot_rtc.valid ? "valid, system clock set" : "unset, waiting for NTP");

  state_.location_name = locations::kHome.name;
  state_.show_seconds = false;  // BOREAS_SECONDS_CLOCK build (D-008) not implemented yet
  wifi_.begin(secrets::kWifiSsid, secrets::kWifiPassword);
  panel_.begin();

  // D-016 proof: the board must hash this scene exactly as the host preview does.
  {
    static MemCanvas check;
    ui::draw_test_card(check.u8g2());
    Serial.printf("preview check: scene test hash %08lx\n", static_cast<unsigned long>(check.hash()));
  }

  redraw();
}

void App::loop() {
  const uint32_t now_ms = millis();
  const Link link_before = state_.link;
  const uint32_t syncs_before = ntp_.sync_count();
  const bool clock_before = clock_valid_;

  wifi_.service(now_ms);
  ntp_.service(now_ms, wifi_.connected());
  if (ntp_.synced()) clock_valid_ = true;

  state_.link = wifi_.connected() ? Link::kUp
                : wifi_.state() == WiFiMgr::State::kConnecting ? Link::kConnecting
                                                                : Link::kDown;
  if (state_.link != link_before)
    Serial.printf("link %d -> %d\n", static_cast<int>(link_before), static_cast<int>(state_.link));

  const bool fetched = service_fetch(time(nullptr));

  // Redraw on the minute and immediately on any change (SPEC.md §Scheduling).
  const bool dirty = fetched || state_.link != link_before || ntp_.sync_count() != syncs_before ||
                     clock_valid_ != clock_before;
  const int64_t minute = current_minute();
  if (schedule::redraw_due(minute, last_minute_, dirty)) redraw();

  delay(10);
}

// Clock minutes once the clock is trusted (redraws land on the minute);
// uptime minutes before that.
int64_t App::current_minute() const {
  return clock_valid_ ? static_cast<int64_t>(time(nullptr)) / 60 : static_cast<int64_t>(millis() / 60000);
}

// One fetch when due. Needs Wi-Fi and a valid clock: TLS checks certificate
// dates (D-017). Timing from schedule::FetchScheduler.
bool App::service_fetch(time_t now) {
  if (!wifi_.connected() || !clock_valid_ || !fetch_schedule_.due(now)) return false;

  ++fetch_count_;
  Serial.printf("fetch %lu: home location, heap free %lu, min ever %lu, largest block %lu\n",
                static_cast<unsigned long>(fetch_count_), static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()), static_cast<unsigned long>(ESP.getMaxAllocHeap()));
  last_fetch_ = meteo_.fetch(locations::kHome.latitude, locations::kHome.longitude, now, state_.weather);

  const time_t done = time(nullptr);
  if (last_fetch_.ok) {
    pressure_.add(state_.weather.current.time, state_.weather.current.pressure_hpa);
    // The fresh data decides the next interval (15 min while an umbrella is advised).
    advisory_ = derived::derive(state_.weather, state_.indoor, pressure_, done,
                                schedule::fetch_interval_s(false, kNightMode))
                    .umbrella;
    fetch_schedule_.on_success(done, advisory_, kNightMode);
  } else {
    fetch_schedule_.on_failure(done);
  }
  Serial.printf("fetch %lu: %s (http %d, %lu ms) %s | heap free %lu, min ever %lu | next in %ld s%s\n",
                static_cast<unsigned long>(fetch_count_), last_fetch_.ok ? "OK" : "FAIL", last_fetch_.http_status,
                static_cast<unsigned long>(last_fetch_.elapsed_ms), last_fetch_.error,
                static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getMinFreeHeap()),
                static_cast<long>(fetch_schedule_.next() - done), advisory_ ? " (umbrella: 15 min)" : "");
  return true;
}

void App::read_sensors() {
  state_.indoor = shtc3_.read();
  state_.battery = battery_.read();
  rtc_reading_ = rtc_.read();
}

// Everything except the models the fetch and sensors already wrote.
void App::update_state(time_t now) {
  PanelState& s = state_;
  s.clock_valid = clock_valid_;
  s.now_utc = now;
  s.rtc_ok = rtc_reading_.valid;
  s.rssi = wifi_.rssi();
  s.last_fetch_utc = s.weather.valid ? s.weather.fetched_utc : 0;
  s.next_fetch_utc = fetch_schedule_.next();
  s.derived = derived::derive(s.weather, s.indoor, pressure_, now, schedule::fetch_interval_s(advisory_, kNightMode));
  s.last_error = last_fetch_.ok ? "" : last_fetch_.error;
}

void App::redraw() {
  ++redraw_count_;
  last_minute_ = current_minute();
  read_sensors();
  update_state(time(nullptr));

  u8g2_t* g = panel_.raw();
  u8g2_ClearBuffer(g);
  ui::draw_frame(g, state_);
  ui::draw_outdoor(g, state_);
  ui::draw_indoor(g, state_);
  ui::draw_readouts(g, state_);
  ui::draw_trace(g, state_);
  ui::draw_rain_meter(g, state_);
  ui::draw_footer(g, state_);
  panel_.end_frame();

  const DerivedModel& d = state_.derived;
  Serial.printf("redraw %lu: send %lu us | in %.1fC %.0f%% | batt %.2fV | rtc %s | %s %s stale=%d trend=%s "
                "rain=%u%% umbrella=%d | heap %lu\n",
                static_cast<unsigned long>(redraw_count_), static_cast<unsigned long>(panel_.last_send_us()),
                static_cast<double>(state_.indoor.temp_c), static_cast<double>(state_.indoor.rh),
                static_cast<double>(state_.battery.volts), state_.rtc_ok ? "OK" : "UNSET", d.condition,
                d.is_night ? "night" : "day", d.stale, trend_name(d.pressure_trend), d.max_prob_12h, d.umbrella,
                static_cast<unsigned long>(ESP.getFreeHeap()));
}
