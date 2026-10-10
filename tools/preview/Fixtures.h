// Fake PanelStates for the host preview. Public example data only: the real
// location never appears here.
#pragma once

#include <cstring>

#include "data/Derived.h"
#include "data/PanelState.h"
#include "data/PressureHistory.h"

namespace fixtures {

namespace detail {

// A 24 h forecast starting at the hour containing `now`, with temperatures
// from `t0` rising by `rise` to a peak 4 h later and falling after.
inline void fill_hourly(WeatherModel& w, time_t now, float t0, float rise, uint8_t prob) {
  w.hourly_count = kHourlyRows;
  const time_t hour0 = now - now % 3600;
  for (int i = 0; i < kHourlyRows; ++i) {
    w.hourly[i].time = hour0 + i * 3600;
    w.hourly[i].temp_c = t0 + rise * (i <= 4 ? i : 8 - i) / 4.0f;
    w.hourly[i].precip_prob = static_cast<uint8_t>(i < 12 ? prob : 10);
    w.hourly[i].weather_code = w.current.weather_code;
  }
}

// prior_hpa > 0 adds a pressure sample 3 h before current.time, so the
// trend arrow shows (SPEC.md §Derived values).
inline void finish(PanelState& s, float prior_hpa = 0.0f) {
  PressureHistory p;
  if (prior_hpa > 0.0f) p.add(s.weather.current.time - 3 * 3600, prior_hpa);
  if (s.weather.valid) p.add(s.weather.current.time, s.weather.current.pressure_hpa);
  s.derived = derived::derive(s.weather, s.indoor, p, s.now_utc, 1800);
}

}  // namespace detail

// Matches the design/ mockup: KINGS LANGLEY, TUE 06 OCT 2026, 13:30:07 BST,
// 14 C light rain, feels 12, high 17, low 9, indoor 21.4 C / 48 %.
inline PanelState typical() {
  PanelState s;
  s.location_name = "KINGS LANGLEY";
  s.clock_valid = true;
  s.now_utc = 1791289807;
  s.rtc_ok = true;
  s.link = Link::kUp;
  s.rssi = -58;
  s.last_fetch_utc = 1791288000;  // 13:00 BST
  s.next_fetch_utc = 1791289800;

  WeatherModel& w = s.weather;
  w.valid = true;
  w.fetched_utc = s.last_fetch_utc;
  w.current = {1791289800, 14.0f, 12.0f, 84, 61, 18.0f, 225, 31.0f, 1009.0f, 2.0f};
  w.daily_count = 2;
  w.daily[0] = {1791241200, 17.0f, 9.0f, 1791267000, 1791307560, 2.4f, 70};  // rise 07:10, set 18:26 BST
  w.daily[1] = {1791327600, 15.0f, 8.0f, 1791353460, 1791393840, 2.0f, 30};
  detail::fill_hourly(w, s.now_utc, 14.0f, 3.0f, 60);

  s.indoor = {true, 21.4f, 48.0f, 21.4f};
  s.battery = {true, 3.9f, 3, false, false};
  detail::finish(s, 1010.5f);  // falling: the mockup's 1009 with a down arrow
  return s;
}

// Worst cases (AV-007). Header: a 14-character name of the widest capital,
// the seconds clock, a wide date (WED 30 SEP 23:59:59 BST). Outdoor: -25 C
// hero, two-digit negatives, a long condition, night icon.
inline PanelState worst() {
  PanelState s = typical();
  s.location_name = "WWWWWWWWWWWWWW";
  s.show_seconds = true;
  s.now_utc = 1790809199;
  WeatherModel& w = s.weather;
  w.fetched_utc = s.now_utc - 600;
  w.current.time = s.now_utc - 600;
  w.current.temp_c = -24.6f;
  w.current.feels_c = -15.6f;
  w.current.weather_code = 67;  // FREEZING RAIN
  w.daily[0].max_c = -10.0f;
  w.daily[0].min_c = -28.0f;
  w.daily[0].date = 1790722800;  // 30 SEP local midnight
  w.daily[0].sunrise = 1790748000;
  w.daily[0].sunset = 1790789000;  // already set: night
  detail::fill_hourly(w, s.now_utc, -24.6f, 6.0f, 100);
  s.indoor = {true, 35.9f, 100.0f, 35.9f};  // HUMID: inverted box, full meter, +60.5 delta
  w.current.wind_kmh = 118.0f;   // readouts worst case: NW 118, gust 140, 100 %, 1035 rising, UV 11 EXTREME
  w.current.wind_dir_deg = 315;
  w.current.gust_kmh = 140.0f;
  w.current.rh = 100;
  w.current.pressure_hpa = 1035.0f;
  w.current.uv = 11.0f;
  detail::finish(s, 1030.0f);
  return s;
}

// Dry afternoon: 20 % rain, mainly clear, neutral advisory, steady pressure.
inline PanelState dry() {
  PanelState s = typical();
  WeatherModel& w = s.weather;
  w.current.weather_code = 1;
  detail::fill_hourly(w, s.now_utc, 14.0f, 3.0f, 20);
  detail::finish(s, 1009.4f);
  return s;
}

// Stale (F-007): fetched 13:00, now 15:30 with the link down, battery low.
inline PanelState stale() {
  PanelState s = typical();
  s.now_utc = 1791288000 + 150 * 60;
  s.link = Link::kDown;
  s.rssi = 0;
  s.next_fetch_utc = s.now_utc + 4 * 60;
  s.battery = {true, 3.32f, 0, true, false};
  detail::finish(s, 1010.5f);
  return s;
}

// No trusted time yet: clock shows dashes.
inline PanelState unset() {
  PanelState s = typical();
  s.clock_valid = false;
  s.rtc_ok = false;
  s.link = Link::kConnecting;
  s.weather = WeatherModel{};  // never fetched
  s.last_fetch_utc = 0;
  s.indoor = IndoorModel{};  // sensor failed
  detail::finish(s);
  return s;
}

struct Named {
  const char* name;
  PanelState (*make)();
};

constexpr Named kAll[] = {{"typical", typical}, {"worst", worst}, {"dry", dry}, {"stale", stale}, {"unset", unset}};

}  // namespace fixtures
