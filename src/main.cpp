// Boreas MS-42 firmware entry point.
//
// Temporary Phase 1 scaffold: proves display/, sensors/ and net/ on the
// board. It reads the sensors, keeps Wi-Fi and NTP running, and redraws a
// placeholder card every minute and on any state change. The drawing here
// moves to ui/ and the timing to app/Scheduler as those modules arrive.

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
#include "sensors/CivilTime.h"
#include "sensors/Rtc.h"
#include "sensors/Shtc3.h"
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

void draw_placeholder() {
  U8G2& c = panel.canvas();
  char line[64];
  c.clearBuffer();
  c.drawFrame(3, 3, 394, 294);
  c.drawFrame(5, 5, 390, 290);
  c.drawBox(5, 5, 390, 20);
  c.setDrawColor(0);
  c.setFont(u8g2_font_7x14B_tr);
  c.drawStr(10, 20, "BOREAS MS-42");
  c.setDrawColor(1);
  c.setFont(u8g2_font_6x12_tr);
  c.drawStr(12, 50, "PHASE 1 - DISPLAY + SENSORS + NET + METEO");
  snprintf(line, sizeof(line), "REDRAW %lu  UPTIME %lu MIN", static_cast<unsigned long>(redraw_count),
           static_cast<unsigned long>(millis() / 60000));
  c.drawStr(12, 66, line);
  snprintf(line, sizeof(line), "LAST SEND %lu MS", static_cast<unsigned long>(panel.last_send_us() / 1000));
  c.drawStr(12, 82, line);

  if (indoor.valid)
    snprintf(line, sizeof(line), "INDOOR %.1f C  %.0f %%RH  (RAW %.1f)", indoor.temp_c, indoor.rh,
             indoor.raw_temp_c);
  else
    snprintf(line, sizeof(line), "INDOOR SENSOR FAIL");
  c.drawStr(12, 106, line);

  snprintf(line, sizeof(line), "BATT %.2f V  %u/4%s%s", batt.volts, batt.cells, batt.low ? "  LOW" : "",
           batt.halt ? "  HALT" : "");
  c.drawStr(12, 122, line);

  if (!clock_reading.present) {
    snprintf(line, sizeof(line), "RTC MISSING");
  } else if (!clock_reading.valid) {
    snprintf(line, sizeof(line), "RTC UNSET  --:--  (WAITING FOR NTP)");
  } else {
    civil::DateTime t = civil::from_utc(clock_reading.utc);
    snprintf(line, sizeof(line), "RTC OK  %04d-%02d-%02d %02d:%02d UTC", t.year, t.month, t.day, t.hour,
             t.minute);
  }
  c.drawStr(12, 138, line);

  const uint32_t now_ms = millis();
  switch (wifi.state()) {
    case WiFiMgr::State::kConnected:
      snprintf(line, sizeof(line), "WIFI OK  %d DBM  RECONNECTS %lu", wifi.rssi(),
               static_cast<unsigned long>(wifi.reconnects()));
      break;
    case WiFiMgr::State::kConnecting:
      snprintf(line, sizeof(line), "WIFI CONNECTING");
      break;
    case WiFiMgr::State::kWaiting:
      snprintf(line, sizeof(line), "WIFI DOWN  RETRY IN %lu S",
               static_cast<unsigned long>(wifi.retry_in_ms(now_ms) / 1000));
      break;
    case WiFiMgr::State::kOff:
      snprintf(line, sizeof(line), "WIFI OFF");
      break;
  }
  c.drawStr(12, 162, line);

  if (ntp.synced())
    snprintf(line, sizeof(line), "NTP OK  SYNCS %lu  RTC DRIFT %ld S", static_cast<unsigned long>(ntp.sync_count()),
             static_cast<long>(ntp.last_drift_s()));
  else
    snprintf(line, sizeof(line), "NTP %s", ntp.first_sync_overdue(now_ms) ? "OVERDUE" : "WAITING");
  c.drawStr(12, 178, line);

  if (clock_valid) {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    strftime(line, sizeof(line), "LOCAL %a %d %b %H:%M %Z", &local);
  } else {
    snprintf(line, sizeof(line), "LOCAL --:--");
  }
  c.drawStr(12, 194, line);

  if (weather.valid) {
    time_t fetched = weather.fetched_utc;
    struct tm local;
    localtime_r(&fetched, &local);
    snprintf(line, sizeof(line), "METEO %.1fC FEELS %.1f WMO %u @ %02d:%02d", weather.current.temp_c,
             weather.current.feels_c, weather.current.weather_code, local.tm_hour, local.tm_min);
  } else {
    snprintf(line, sizeof(line), "METEO NO DATA YET");
  }
  c.drawStr(12, 218, line);
  if (fetch_count == 0)
    snprintf(line, sizeof(line), "FETCH WAITING FOR WIFI + CLOCK");
  else if (last_fetch.ok)
    snprintf(line, sizeof(line), "FETCH OK %lu MS  HI %.1f LO %.1f", static_cast<unsigned long>(last_fetch.elapsed_ms),
             weather.daily[0].max_c, weather.daily[0].min_c);
  else
    snprintf(line, sizeof(line), "FETCH FAIL %s", last_fetch.error);
  c.drawStr(12, 234, line);

  const DerivedModel d = derived::derive(weather, indoor, pressure, time(nullptr), config::kFetchIntervalMs / 1000);
  static const char* const kTrend[] = {"?", "UP", "STEADY", "DOWN"};
  snprintf(line, sizeof(line), "%s %s%s  WIND %s  UV %s  P %s", d.condition, d.is_night ? "NIGHT" : "DAY",
           d.stale ? " STALE" : "", d.wind_compass, d.uv_band, kTrend[static_cast<int>(d.pressure_trend)]);
  c.drawStr(12, 250, line);
  snprintf(line, sizeof(line), "DEW IN %.1f  D %+.1f  %s  RH %u/10  RAIN %u%% %u/12%s", d.indoor_dew_c, d.delta_c,
           derived::comfort_text(d.comfort), d.rh_segments, d.max_prob_12h, d.rain_segments,
           d.umbrella ? " UMBRELLA" : "");
  c.drawStr(12, 266, line);
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
  draw_placeholder();
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
  // the host preview does; the panel shows it for comparison with the PNG.
  {
    static MemCanvas check;
    ui::draw_test_card(check.u8g2());
    Serial.printf("preview check: scene test hash %08lx\n", static_cast<unsigned long>(check.hash()));
    ui::draw_test_card(panel.raw());
    panel.end_frame();
    delay(10000);
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
