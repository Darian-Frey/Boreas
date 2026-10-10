// Boreas MS-42 firmware entry point.
//
// Temporary Phase 1 scaffold until app/ exists: reads the sensors, keeps
// Wi-Fi, NTP and the forecast fetch running, builds the PanelState, and
// redraws the Main page (regions as they land in ui/) every minute and on
// any state change. Detailed status goes to serial. The timing moves to
// app/Scheduler.

#include <Arduino.h>
#include <Wire.h>
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

#include "data/Derived.h"
#include "data/Models.h"
#include "data/PressureHistory.h"
#include "display/MemCanvas.h"
#include "display/St7305Panel.h"
#include "net/Backoff.h"
#include "net/MeteoClient.h"
#include "net/NtpSync.h"
#include "net/WiFiMgr.h"
#include "config.h"
#include "pins.h"
#include "sensors/Battery.h"
#include "sensors/Rtc.h"
#include "sensors/Shtc3.h"
#include "data/PanelState.h"
#include "ui/Frame.h"
#include "ui/Indoor.h"
#include "ui/Outdoor.h"
#include "ui/Readouts.h"
#include "ui/TestCard.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kRedrawIntervalMs = 60000;  // SPEC.md §Scheduling

St7305Panel panel;
Shtc3 shtc3(Wire);
Rtc rtc(Wire);
Battery battery;
WiFiMgr wifi;
NtpSync ntp(rtc);
bool clock_valid = false;  // system clock set from a valid RTC or NTP
MeteoClient meteo_client;
WeatherModel weather;
MeteoClient::Result last_fetch;
Backoff fetch_backoff(config::kFetchRetryInitialMs, config::kFetchRetryCapMs);
uint32_t next_fetch_ms = 0;
uint32_t fetch_count = 0;
PressureHistory pressure;

IndoorModel indoor;
BatteryModel batt;
RtcReading clock_reading;
uint32_t last_redraw_ms = 0;
uint32_t redraw_count = 0;

// The single PanelState (app/ will own it once the Scheduler exists).
PanelState state;

void build_state() {
  PanelState& s = state;
  s.location_name = locations::kHome.name;
  s.clock_valid = clock_valid;
  s.now_utc = time(nullptr);
  s.rtc_ok = clock_reading.valid;
  s.link = wifi.connected() ? Link::kUp
           : wifi.state() == WiFiMgr::State::kConnecting ? Link::kConnecting
                                                          : Link::kDown;
  s.rssi = wifi.rssi();
  s.weather = weather;
  s.indoor = indoor;
  s.battery = batt;
  s.last_fetch_utc = weather.valid ? weather.fetched_utc : 0;
  s.next_fetch_utc = fetch_count > 0 ? s.now_utc + static_cast<int32_t>(next_fetch_ms - millis()) / 1000 : 0;
  s.derived = derived::derive(weather, indoor, pressure, s.now_utc, config::kFetchIntervalMs / 1000);
  s.last_error = last_fetch.ok ? "" : last_fetch.error;

  const DerivedModel& d = s.derived;
  static const char* const kTrend[] = {"?", "UP", "STEADY", "DOWN"};
  Serial.printf("derived: %s icon=%d night=%d stale=%d wind=%s uv=%s trend=%s dew_in=%.2f delta=%+.2f comfort=%s "
                "rh_seg=%u first=%u trace=%u min=%.1f max=%.1f peak=%u rain=%u%% seg=%u umbrella=%d\n",
                d.condition, static_cast<int>(d.icon), d.is_night, d.stale, d.wind_compass, d.uv_band,
                kTrend[static_cast<int>(d.pressure_trend)], d.indoor_dew_c, d.delta_c, derived::comfort_text(d.comfort),
                d.rh_segments, d.first_hour, d.trace_count, d.trace_min_c, d.trace_max_c, d.trace_peak, d.max_prob_12h,
                d.rain_segments, d.umbrella);
}

// One fetch when due: Wi-Fi up and clock valid (TLS checks certificate
// dates). 30 min after success; 1, 2, 4, 8, then 15 min after failure.
bool service_fetch(uint32_t now_ms) {
  if (!wifi.connected() || !clock_valid) return false;
  if (fetch_count > 0 && static_cast<int32_t>(now_ms - next_fetch_ms) < 0) return false;

  ++fetch_count;
  const uint32_t heap_before = ESP.getFreeHeap();
  Serial.printf("fetch %lu: home location, heap free %lu, min ever %lu, largest block %lu\n",
                static_cast<unsigned long>(fetch_count), static_cast<unsigned long>(heap_before),
                static_cast<unsigned long>(ESP.getMinFreeHeap()), static_cast<unsigned long>(ESP.getMaxAllocHeap()));
  last_fetch = meteo_client.fetch(locations::kHome.latitude, locations::kHome.longitude, time(nullptr), weather);
  const uint32_t delay_ms = last_fetch.ok ? config::kFetchIntervalMs : fetch_backoff.next();
  if (last_fetch.ok) {
    fetch_backoff.reset();
    pressure.add(weather.current.time, weather.current.pressure_hpa);
  }
  next_fetch_ms = millis() + delay_ms;
  Serial.printf("fetch %lu: %s (http %d, %lu ms) %s | heap free %lu, min ever %lu | next in %lu s\n",
                static_cast<unsigned long>(fetch_count), last_fetch.ok ? "OK" : "FAIL", last_fetch.http_status,
                static_cast<unsigned long>(last_fetch.elapsed_ms), last_fetch.error,
                static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getMinFreeHeap()),
                static_cast<unsigned long>(delay_ms / 1000));
  return true;
}

void read_sensors() {
  indoor = shtc3.read();
  batt = battery.read();
  clock_reading = rtc.read();
  Serial.printf("indoor valid=%d T=%.2f RH=%.1f | batt %.2f V cells=%u low=%d halt=%d | rtc present=%d valid=%d\n",
                indoor.valid, indoor.temp_c, indoor.rh, batt.volts, batt.cells, batt.low, batt.halt,
                clock_reading.present, clock_reading.valid);
}

void redraw() {
  ++redraw_count;
  read_sensors();
  build_state();
  u8g2_t* g = panel.raw();
  u8g2_ClearBuffer(g);
  ui::draw_frame(g, state);
  ui::draw_outdoor(g, state);
  ui::draw_indoor(g, state);
  ui::draw_readouts(g, state);
  panel.end_frame();
  Serial.printf("redraw %lu: send %lu us\n", static_cast<unsigned long>(redraw_count),
                static_cast<unsigned long>(panel.last_send_us()));
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  // An unread USB CDC port must drop output, not stall loop() (AV-011).
  Serial.setTxTimeoutMs(0);
  Serial.println("\nBoreas MS-42");

  Wire.begin(pins::kI2cSda, pins::kI2cScl);
  battery.begin();
  Serial.printf("SHTC3 %s\n", shtc3.begin() ? "OK" : "NOT FOUND");

  // D-007: show a correct clock before Wi-Fi, but only from a trusted RTC (D-015).
  ntp.begin();
  RtcReading boot_rtc = rtc.read();
  if (boot_rtc.valid) {
    struct timeval tv = {boot_rtc.utc, 0};
    settimeofday(&tv, nullptr);
    clock_valid = true;
  }
  Serial.printf("RTC at boot: %s\n", boot_rtc.valid ? "valid, system clock set" : "unset, waiting for NTP");

  wifi.begin(secrets::kWifiSsid, secrets::kWifiPassword);

  panel.begin();

  // D-016 proof: the same scene drawn into a MemCanvas must hash exactly as
  // the host preview does.
  {
    static MemCanvas check;
    ui::draw_test_card(check.u8g2());
    Serial.printf("preview check: scene test hash %08lx\n", static_cast<unsigned long>(check.hash()));
  }
  redraw();
  last_redraw_ms = millis();
}

void loop() {
  const uint32_t now_ms = millis();
  const WiFiMgr::State wifi_before = wifi.state();
  const uint32_t syncs_before = ntp.sync_count();

  wifi.service(now_ms);
  ntp.service(now_ms, wifi.connected());
  if (ntp.synced()) clock_valid = true;

  if (wifi.state() != wifi_before) {
    Serial.printf("wifi state %d -> %d\n", static_cast<int>(wifi_before), static_cast<int>(wifi.state()));
  }
  // Redraw on the minute, and immediately on any data change (SPEC.md §Scheduling).
  const bool fetched = service_fetch(now_ms);
  const bool changed = wifi.connected() != (wifi_before == WiFiMgr::State::kConnected) ||
                       ntp.sync_count() != syncs_before || fetched;
  if (changed || now_ms - last_redraw_ms >= kRedrawIntervalMs) {
    last_redraw_ms = now_ms;
    redraw();
  }
  delay(10);
}
