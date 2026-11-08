#include "app/App.h"

#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <sys/time.h>

#include <cstdio>
#include <cstring>

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

const char* reset_reason_name(esp_reset_reason_t r) {
  static const char* const kNames[] = {"UNKNOWN", "POWERON", "EXT",      "SW",   "PANIC", "INT_WDT",
                                       "TASK_WDT", "WDT",    "DEEPSLEEP", "BROWNOUT", "SDIO", "USB",
                                       "JTAG",    "EFUSE",   "PWR_GLITCH", "CPU_LOCKUP"};
  const int i = static_cast<int>(r);
  return i >= 0 && i < static_cast<int>(sizeof(kNames) / sizeof(kNames[0])) ? kNames[i] : "?";
}

constexpr const char* kHealthHeader =
    "utc,local,uptime_s,event,free_heap,min_free_heap,max_block,fetch_ok,fetch_fail,wifi_reconnects,ntp_syncs,"
    "indoor_c,batt_v,note";

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

  Serial.printf("SD %s\n", sd_.begin() ? "mounted" : "not available (health log off until a card is present)");

  state_.location_name = locations::kHome.name;
  state_.show_seconds = false;  // BOREAS_SECONDS_CLOCK build (D-008) not implemented yet
  wifi_.begin(secrets::kWifiSsid, secrets::kWifiPassword);
  panel_.begin();
  te_seen_ms_ = millis();

  // D-016 proof: the board must hash this scene exactly as the host preview does.
  {
    static MemCanvas check;
    ui::draw_test_card(check.u8g2());
    Serial.printf("preview check: scene test hash %08lx\n", static_cast<unsigned long>(check.hash()));
  }

  redraw();
  // After the first redraw so the sensor columns are filled in.
  log_health("boot", reset_reason_name(esp_reset_reason()));
  last_health_ms_ = millis();

  // Watchdog: a hung loop resets the chip with TASK_WDT instead of failing
  // silently. Arduino feeds it once per loop() pass once enabled.
  esp_task_wdt_config_t wdt = {};
  wdt.timeout_ms = config::kWatchdogTimeoutS * 1000;
  wdt.idle_core_mask = 1 << 0;  // keep watching CPU0's idle task, as the core default does
  wdt.trigger_panic = true;
  esp_task_wdt_reconfigure(&wdt);
  enableLoopWDT();
}

void App::loop() {
  service_serial();
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
  service_wifi_log(link_before, now_ms);

  const bool fetched = service_fetch(time(nullptr));

  // Redraw on the minute and immediately on any change (SPEC.md §Scheduling).
  const bool dirty = fetched || state_.link != link_before || ntp_.sync_count() != syncs_before ||
                     clock_valid_ != clock_before;
  const int64_t minute = current_minute();
  if (schedule::redraw_due(minute, last_minute_, dirty)) redraw();

  service_panel(millis());

  if (now_ms - last_health_ms_ >= config::kHealthLogIntervalS * 1000UL) {
    last_health_ms_ = now_ms;
    const uint32_t te = panel_.te_pulses();
    char note[40];
    snprintf(note, sizeof(note), "te=%lu rssi=%d", static_cast<unsigned long>(te - te_at_last_tick_),
             static_cast<int>(wifi_.last_rssi()));
    te_at_last_tick_ = te;
    log_health("tick", note);
  }

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
    ++fetch_ok_;
  } else {
    fetch_schedule_.on_failure(done);
    ++fetch_fail_;
    log_health("fetch_fail", last_fetch_.error);
  }
  fetch_warnings(done);
  Serial.printf("fetch %lu: %s (http %d, %lu ms) %s | heap free %lu, min ever %lu | next in %ld s%s\n",
                static_cast<unsigned long>(fetch_count_), last_fetch_.ok ? "OK" : "FAIL", last_fetch_.http_status,
                static_cast<unsigned long>(last_fetch_.elapsed_ms), last_fetch_.error,
                static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getMinFreeHeap()),
                static_cast<long>(fetch_schedule_.next() - done), advisory_ ? " (umbrella: 15 min)" : "");
  return true;
}

// Met Office warnings for the configured area (F-019), in the forecast slot.
void App::fetch_warnings(time_t now) {
  if (warnings_tried_ != 0 && now - warnings_tried_ < static_cast<time_t>(config::kWarningsIntervalS)) return;
  warnings_tried_ = now;
  const WarningsClient::Result r =
      warnings_.fetch(locations::kWarningRegion, locations::kWarningArea, now, state_.warnings);
  Serial.printf("warnings: %s (http %d, %lu bytes, %lu ms) %s | %u for the area\n", r.ok ? "OK" : "FAIL",
                r.http_status, static_cast<unsigned long>(r.bytes), static_cast<unsigned long>(r.elapsed_ms),
                r.error, static_cast<unsigned>(state_.warnings.count));
  if (!r.ok) log_health("warn_fail", r.error);
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
  s.derived.warning = derived::select_warning(s.warnings, now, &s.derived.warning_active);
  s.last_error = last_fetch_.ok ? "" : last_fetch_.error;

  // Health log: one line whenever the warning on the panel changes.
  char shown[sizeof(warning_logged_)] = "none";
  if (s.derived.warning >= 0) {
    static const char* const kLevel[] = {"", "yellow", "amber", "red"};
    const Warning& w = s.warnings.items[s.derived.warning];
    snprintf(shown, sizeof(shown), "%s %s %s", kLevel[static_cast<int>(w.level)], w.hazard,
             s.derived.warning_active ? "now" : "soon");
  }
  if (std::strcmp(shown, warning_logged_) != 0) {
    if (warning_logged_[0] != '\0' || std::strcmp(shown, "none") != 0) log_health("warning", shown);
    snprintf(warning_logged_, sizeof(warning_logged_), "%s", shown);
  }
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

// Panel heartbeat (D-020, BUG-004). TE pulses at >= 1 Hz while the panel is
// alive. If none arrive for kPanelTeTimeoutMs the panel is logged as lost and
// re-initialised, again every kPanelRecoverIntervalS while it stays silent;
// when pulses return the recovery is logged with how long it took.
void App::service_panel(uint32_t now_ms) {
  const uint32_t te = panel_.te_pulses();
  char note[48];
  if (te != te_seen_) {
    te_seen_ = te;
    te_seen_ms_ = now_ms;
    if (panel_lost_) {
      panel_lost_ = false;
      snprintf(note, sizeof(note), "after=%lus reinits=%lu", static_cast<unsigned long>((now_ms - panel_lost_ms_) / 1000),
               static_cast<unsigned long>(panel_recover_tries_));
      log_health("panel_back", note);
    }
    return;
  }
  if (now_ms - te_seen_ms_ < config::kPanelTeTimeoutMs) return;

  if (!panel_lost_) {
    panel_lost_ = true;
    panel_lost_ms_ = now_ms;
    panel_recover_tries_ = 0;
    snprintf(note, sizeof(note), "no TE for %lus", static_cast<unsigned long>((now_ms - te_seen_ms_) / 1000));
    log_health("panel_lost", note);
  }
  if (panel_recover_tries_ == 0 || now_ms - panel_recover_ms_ >= config::kPanelRecoverIntervalS * 1000UL) {
    ++panel_recover_tries_;
    panel_recover_ms_ = now_ms;
    const uint32_t took = panel_.reinit();
    redraw();  // put a frame straight back on the re-initialised panel
    snprintf(note, sizeof(note), "try=%lu took=%lums", static_cast<unsigned long>(panel_recover_tries_),
             static_cast<unsigned long>(took));
    log_health("panel_reinit", note);
  }
}

// Wi-Fi diagnostics for the health log: every drop with its reason and the
// last signal strength, every failed reconnect attempt with its reason (0 =
// timed out), and every recovery with the outage length.
void App::service_wifi_log(Link link_before, uint32_t now_ms) {
  char note[48];
  const bool up = state_.link == Link::kUp, was_up = link_before == Link::kUp;
  if (was_up && !up) {
    wifi_down_ms_ = now_ms;
    wifi_fails_at_down_ = wifi_.failed_attempts();
    snprintf(note, sizeof(note), "reason=%u rssi=%d", static_cast<unsigned>(wifi_.last_drop_reason()),
             static_cast<int>(wifi_.last_rssi()));
    log_health("wifi_down", note);
  }
  if (wifi_.failed_attempts() != wifi_fails_seen_) {
    wifi_fails_seen_ = wifi_.failed_attempts();
    snprintf(note, sizeof(note), "reason=%u", static_cast<unsigned>(wifi_.last_fail_reason()));
    log_health("wifi_fail", note);
  }
  if (!was_up && up && wifi_down_ms_ != 0) {
    snprintf(note, sizeof(note), "after=%lus attempts=%lu",
             static_cast<unsigned long>((now_ms - wifi_down_ms_) / 1000),
             static_cast<unsigned long>(wifi_.failed_attempts() - wifi_fails_at_down_ + 1));
    log_health("wifi_up", note);
    wifi_down_ms_ = 0;
  }
}

// One CSV line to the TF card (SPEC.md §Health log) and serial. Not in the
// redraw path; stack buffers only.
void App::log_health(const char* event, const char* note) {
  char local[24] = "-";
  const time_t now = time(nullptr);
  if (clock_valid_) {
    struct tm t;
    localtime_r(&now, &t);
    strftime(local, sizeof(local), "%Y-%m-%d %H:%M:%S", &t);
  }
  char clean[64];
  snprintf(clean, sizeof(clean), "%s", note ? note : "");
  for (char* c = clean; *c; ++c)
    if (*c == ',' || *c == '\n' || *c == '\r') *c = ';';

  char line[256];
  snprintf(line, sizeof(line), "%lld,%s,%lu,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%.1f,%.2f,%s",
           clock_valid_ ? static_cast<long long>(now) : 0LL, local, static_cast<unsigned long>(millis() / 1000), event,
           static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getMinFreeHeap()),
           static_cast<unsigned long>(ESP.getMaxAllocHeap()), static_cast<unsigned long>(fetch_ok_),
           static_cast<unsigned long>(fetch_fail_), static_cast<unsigned long>(wifi_.reconnects()),
           static_cast<unsigned long>(ntp_.sync_count()), static_cast<double>(state_.indoor.temp_c),
           static_cast<double>(state_.battery.volts), clean);
  const bool written = sd_.append(config::kHealthLogPath, kHealthHeader, line);
  Serial.printf("health%s: %s\n", written ? "" : " (not written)", line);
}

// Serial commands (newline-terminated). `dump`: stream the health log as
//   BEGIN <path> <size>\n<file bytes>\nEND <bytes sent>\n
// `wdt-test`: hang loop() so the watchdog resets the chip.
// `panel-test`: put the panel to sleep so the TE monitor must recover it.
// tools/sdlog.py drives this. While dumping, writes wait for the host
// instead of being dropped (AV-011 drops them only when nobody is reading).
void App::service_serial() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) break;
    if (c != '\n' && c != '\r') {
      if (cmd_len_ + 1 < sizeof(cmd_)) cmd_[cmd_len_++] = static_cast<char>(c);
      continue;
    }
    cmd_[cmd_len_] = '\0';
    const bool is_dump = std::strcmp(cmd_, "dump") == 0;
    const bool is_wdt_test = std::strcmp(cmd_, "wdt-test") == 0;
    const bool is_panel_test = std::strcmp(cmd_, "panel-test") == 0;
    const bool had_cmd = cmd_len_ > 0;
    cmd_len_ = 0;
    if (!had_cmd) continue;
    if (is_wdt_test) {
      // Proves the watchdog: hang loop() until it resets the chip (TASK_WDT).
      Serial.printf("wdt-test: hanging loop(); expect a TASK_WDT reset in %lu s\n",
                    static_cast<unsigned long>(config::kWatchdogTimeoutS));
      Serial.flush();
      for (;;) delay(100);
    }
    if (is_panel_test) {
      // Proves the D-020 monitor: silence the panel; expect panel_lost, a
      // re-init and panel_back in the health log within ~6 s.
      Serial.println("panel-test: putting the panel to sleep");
      panel_.sleep_for_test();
      continue;
    }
    if (!is_dump) {
      Serial.printf("unknown command: %s (try: dump, wdt-test, panel-test)\n", cmd_);
      continue;
    }
    Serial.setTxTimeoutMs(500);
    Serial.printf("BEGIN %s %ld\n", config::kHealthLogPath, static_cast<long>(sd_.size(config::kHealthLogPath)));
    const int32_t sent = sd_.dump(config::kHealthLogPath, Serial);
    Serial.printf("\nEND %ld\n", static_cast<long>(sent));
    Serial.flush();
    Serial.setTxTimeoutMs(0);
  }
}
