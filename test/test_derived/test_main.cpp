// Host-native tests for src/data/Derived and PressureHistory (pio test -e native).

#include <unity.h>

#include <cmath>
#include <cstring>

#include "data/Derived.h"

void setUp() {}
void tearDown() {}

// 2026-10-10 12:00 UTC and a matching synthetic forecast.
constexpr time_t kNoon = 1791633600;
constexpr time_t kHour = 3600;

static WeatherModel make_weather() {
  WeatherModel w;
  w.valid = true;
  w.fetched_utc = kNoon;
  w.current.time = kNoon;
  w.current.temp_c = 14.0f;
  w.current.rh = 84;
  w.current.weather_code = 61;
  w.current.wind_dir_deg = 225;
  w.current.uv = 2.0f;
  w.daily_count = 2;
  w.daily[0].date = kNoon - 13 * kHour;  // local midnight (BST)
  w.daily[0].sunrise = kNoon - 5 * kHour;
  w.daily[0].sunset = kNoon + 5 * kHour;
  w.daily[1].date = w.daily[0].date + 24 * kHour;
  w.daily[1].sunrise = w.daily[0].sunrise + 24 * kHour;
  w.daily[1].sunset = w.daily[0].sunset + 24 * kHour;
  w.hourly_count = 24;
  for (int i = 0; i < 24; ++i) {
    w.hourly[i].time = kNoon + i * kHour;
    w.hourly[i].temp_c = 14.0f + (i <= 4 ? i : 8 - i);  // peaks at +4 h (18 C)
  }
  return w;
}

static IndoorModel make_indoor(float t, float rh) {
  IndoorModel in;
  in.valid = true;
  in.temp_c = t;
  in.rh = rh;
  in.raw_temp_c = t;
  return in;
}

// ---------------------------------------------------------------- Pure helpers

void test_dew_point_magnus() {
  // Reference values from the SPEC.md formula evaluated in Python.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.255f, derived::dew_point_c(20.0f, 50.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.931f, derived::dew_point_c(21.4f, 48.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, derived::dew_point_c(0.0f, 100.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -12.797f, derived::dew_point_c(-10.0f, 80.0f));
  TEST_ASSERT_FALSE(std::isnan(derived::dew_point_c(20.0f, 0.0f)));  // clamped, not ln(0)
}

void test_comfort_bands_and_priority() {
  TEST_ASSERT_EQUAL(Comfort::kNominal, derived::comfort(21.0f, 45.0f));
  TEST_ASSERT_EQUAL(Comfort::kNominal, derived::comfort(18.0f, 30.0f));  // inclusive edges
  TEST_ASSERT_EQUAL(Comfort::kNominal, derived::comfort(24.0f, 60.0f));
  TEST_ASSERT_EQUAL(Comfort::kDry, derived::comfort(21.0f, 29.9f));
  TEST_ASSERT_EQUAL(Comfort::kHumid, derived::comfort(21.0f, 60.1f));
  TEST_ASSERT_EQUAL(Comfort::kCold, derived::comfort(17.9f, 45.0f));
  TEST_ASSERT_EQUAL(Comfort::kWarm, derived::comfort(24.1f, 45.0f));
  TEST_ASSERT_EQUAL(Comfort::kHumid, derived::comfort(15.0f, 70.0f));  // RH wins over T
  TEST_ASSERT_EQUAL_STRING("NOMINAL", derived::comfort_text(Comfort::kNominal));
}

void test_wmo_mapping() {
  TEST_ASSERT_EQUAL_STRING("CLEAR", derived::condition_text(0));
  TEST_ASSERT_EQUAL_STRING("PARTLY CLOUDY", derived::condition_text(2));
  TEST_ASSERT_EQUAL_STRING("FOG", derived::condition_text(48));
  TEST_ASSERT_EQUAL_STRING("DRIZZLE", derived::condition_text(57));
  TEST_ASSERT_EQUAL_STRING("LIGHT RAIN", derived::condition_text(80));
  TEST_ASSERT_EQUAL_STRING("HEAVY RAIN", derived::condition_text(65));
  TEST_ASSERT_EQUAL_STRING("FREEZING RAIN", derived::condition_text(67));
  TEST_ASSERT_EQUAL_STRING("SNOW", derived::condition_text(86));
  TEST_ASSERT_EQUAL_STRING("THUNDERSTORM", derived::condition_text(99));
  TEST_ASSERT_EQUAL_STRING("---", derived::condition_text(4));
  TEST_ASSERT_EQUAL(Icon::kSunCloud, derived::day_icon(1));
  TEST_ASSERT_EQUAL(Icon::kRainIce, derived::day_icon(66));
  TEST_ASSERT_EQUAL(Icon::kNone, derived::day_icon(42));
  TEST_ASSERT_EQUAL(Icon::kMoon, derived::night_variant(Icon::kSun));
  TEST_ASSERT_EQUAL(Icon::kMoonCloud, derived::night_variant(Icon::kSunCloud));
  TEST_ASSERT_EQUAL(Icon::kRain, derived::night_variant(Icon::kRain));
}

void test_condition_text_fits_column() {
  // EXT column condition line: <= 16 characters (SPEC.md §Layout).
  for (int code = 0; code < 100; ++code)
    TEST_ASSERT_LESS_OR_EQUAL(16, strlen(derived::condition_text(static_cast<uint8_t>(code))));
}

void test_compass8() {
  TEST_ASSERT_EQUAL_STRING("N", derived::compass8(0));
  TEST_ASSERT_EQUAL_STRING("N", derived::compass8(22));
  TEST_ASSERT_EQUAL_STRING("NE", derived::compass8(23));
  TEST_ASSERT_EQUAL_STRING("SW", derived::compass8(225));
  TEST_ASSERT_EQUAL_STRING("NW", derived::compass8(337));
  TEST_ASSERT_EQUAL_STRING("N", derived::compass8(338));
  TEST_ASSERT_EQUAL_STRING("N", derived::compass8(360));
}

void test_uv_band() {
  TEST_ASSERT_EQUAL_STRING("LOW", derived::uv_band(0.0f));
  TEST_ASSERT_EQUAL_STRING("LOW", derived::uv_band(2.4f));
  TEST_ASSERT_EQUAL_STRING("MOD", derived::uv_band(2.5f));
  TEST_ASSERT_EQUAL_STRING("HIGH", derived::uv_band(7.0f));
  TEST_ASSERT_EQUAL_STRING("V.HIGH", derived::uv_band(10.0f));
  TEST_ASSERT_EQUAL_STRING("EXTREME", derived::uv_band(11.0f));
}

void test_meter_segments() {
  TEST_ASSERT_EQUAL_UINT8(5, derived::meter_segments(48.0f, 10));  // mockup: RH 48 % -> 5 of 10
  TEST_ASSERT_EQUAL_UINT8(7, derived::meter_segments(60.0f, 12));  // mockup: 60 % -> 7 of 12
  TEST_ASSERT_EQUAL_UINT8(0, derived::meter_segments(0.0f, 12));
  TEST_ASSERT_EQUAL_UINT8(1, derived::meter_segments(5.0f, 12));
  TEST_ASSERT_EQUAL_UINT8(12, derived::meter_segments(100.0f, 12));
  TEST_ASSERT_EQUAL_UINT8(12, derived::meter_segments(130.0f, 12));
}

void test_stale() {
  WeatherModel w = make_weather();
  TEST_ASSERT_FALSE(derived::is_stale(w, kNoon + 3600, 1800));      // exactly 2x: not yet
  TEST_ASSERT_TRUE(derived::is_stale(w, kNoon + 3601, 1800));
  w.valid = false;
  TEST_ASSERT_TRUE(derived::is_stale(w, kNoon, 1800));
}

// ---------------------------------------------------------------- PressureHistory

void test_pressure_trend() {
  PressureHistory p;
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kUnknown, p.trend());
  for (int i = 0; i <= 6; ++i) p.add(kNoon + i * 1800, 1010.0f - i * 0.3f);  // -1.8 hPa over 3 h
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kFalling, p.trend());

  PressureHistory up;
  up.add(kNoon, 1000.0f);
  up.add(kNoon + 3 * kHour, 1001.0f);  // exactly +1.0
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kRising, up.trend());

  PressureHistory flat;
  flat.add(kNoon, 1000.0f);
  flat.add(kNoon + 3 * kHour, 1000.9f);
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kSteady, flat.trend());
}

void test_pressure_needs_three_hours() {
  PressureHistory p;
  p.add(kNoon, 1000.0f);
  p.add(kNoon + 2 * kHour, 1005.0f);  // 2 h: outside 2.5-3.5 h
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kUnknown, p.trend());
  p.add(kNoon + 3 * kHour + 20 * 60, 1005.0f);  // 3 h 20 min from the first: inside tolerance
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kRising, p.trend());
}

void test_pressure_mixed_intervals_and_duplicates() {
  PressureHistory p;
  p.add(kNoon, 1000.0f);
  p.add(kNoon + 900, 1000.0f);  // 15 min fetches...
  p.add(kNoon + 1800, 1000.0f);
  p.add(kNoon + 1800, 1000.2f);  // same bucket fetched twice: replaced, not added
  TEST_ASSERT_EQUAL(3, p.count());
  p.add(kNoon + 900, 999.0f);  // older than latest: ignored
  TEST_ASSERT_EQUAL(3, p.count());
  p.add(kNoon + 3 * kHour + 900, 998.5f);  // ...then a 60 min gap
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kFalling, p.trend());  // vs the 12:15 sample
}

void test_pressure_ring_wraps() {
  PressureHistory p;
  for (int i = 0; i < 40; ++i) p.add(kNoon + i * 1800, 1000.0f + i * 0.5f);  // +3 hPa / 3 h
  TEST_ASSERT_EQUAL(PressureHistory::kSlots, p.count());
  TEST_ASSERT_EQUAL(PressureHistory::Trend::kRising, p.trend());
}

// ---------------------------------------------------------------- derive()

void test_derive_day_values() {
  const WeatherModel w = make_weather();
  PressureHistory p;
  const DerivedModel d = derived::derive(w, make_indoor(21.4f, 48.0f), p, kNoon + 600, 1800);
  TEST_ASSERT_TRUE(d.weather_valid);
  TEST_ASSERT_FALSE(d.stale);
  TEST_ASSERT_FALSE(d.is_night);
  TEST_ASSERT_EQUAL_STRING("LIGHT RAIN", d.condition);
  TEST_ASSERT_EQUAL(Icon::kRain, d.icon);
  TEST_ASSERT_EQUAL_STRING("SW", d.wind_compass);
  TEST_ASSERT_EQUAL_STRING("LOW", d.uv_band);
  TEST_ASSERT_TRUE(d.delta_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.4f, d.delta_c);  // mockup: +7.4
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.931f, d.indoor_dew_c);
  TEST_ASSERT_EQUAL(Comfort::kNominal, d.comfort);
  TEST_ASSERT_EQUAL_UINT8(5, d.rh_segments);
  TEST_ASSERT_EQUAL_UINT8(0, d.first_hour);
  TEST_ASSERT_EQUAL_UINT8(13, d.trace_count);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.0f, d.trace_max_c);
  TEST_ASSERT_EQUAL_UINT8(4, d.trace_peak);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, d.trace_min_c);  // +12 h: 14 + 8 - 12
}

void test_derive_night_icon() {
  WeatherModel w = make_weather();
  w.current.weather_code = 0;
  PressureHistory p;
  const DerivedModel d = derived::derive(w, make_indoor(20, 45), p, kNoon + 6 * kHour, 1800);
  TEST_ASSERT_TRUE(d.is_night);
  TEST_ASSERT_EQUAL(Icon::kMoon, d.icon);
}

void test_derive_after_midnight_uses_tomorrow_row() {
  WeatherModel w = make_weather();
  w.current.weather_code = 1;
  PressureHistory p;
  // 08:00 next day local: after tomorrow's sunrise (07:00 local), so day.
  const time_t t = w.daily[1].sunrise + kHour;
  const DerivedModel d = derived::derive(w, make_indoor(20, 45), p, t, 1800);
  TEST_ASSERT_FALSE(d.is_night);
  TEST_ASSERT_EQUAL(Icon::kSunCloud, d.icon);
}

void test_derive_skips_past_hours() {
  WeatherModel w = make_weather();
  PressureHistory p;
  // 45 min after a fetch that was 2 h old: rows 0 and 1 are in the past.
  const DerivedModel d = derived::derive(w, make_indoor(20, 45), p, kNoon + 2 * kHour + 45 * 60, 3 * 3600);
  TEST_ASSERT_EQUAL_UINT8(2, d.first_hour);
  TEST_ASSERT_EQUAL_UINT8(13, d.trace_count);
  TEST_ASSERT_EQUAL_UINT8(2, d.trace_peak);  // peak still at 16:00, now offset 2
}

void test_derive_umbrella_by_probability() {
  WeatherModel w = make_weather();
  w.hourly[11].precip_prob = 50;  // last hour inside the 12 h window
  w.hourly[12].precip_prob = 90;  // outside it
  PressureHistory p;
  const DerivedModel d = derived::derive(w, make_indoor(20, 45), p, kNoon, 1800);
  TEST_ASSERT_EQUAL_UINT8(50, d.max_prob_12h);
  TEST_ASSERT_EQUAL_UINT8(6, d.rain_segments);
  TEST_ASSERT_TRUE(d.umbrella);
}

void test_derive_umbrella_by_amount_only() {
  WeatherModel w = make_weather();
  w.hourly[3].precip_prob = 30;
  w.hourly[3].precip_mm = 0.5f;
  PressureHistory p;
  const DerivedModel d = derived::derive(w, make_indoor(20, 45), p, kNoon, 1800);
  TEST_ASSERT_TRUE(d.umbrella);

  w.hourly[3].precip_mm = 0.4f;
  const DerivedModel dry = derived::derive(w, make_indoor(20, 45), p, kNoon, 1800);
  TEST_ASSERT_FALSE(dry.umbrella);
}

void test_derive_without_data() {
  WeatherModel w;  // never fetched
  IndoorModel in;  // sensor failed
  PressureHistory p;
  const DerivedModel d = derived::derive(w, in, p, kNoon, 1800);
  TEST_ASSERT_FALSE(d.weather_valid);
  TEST_ASSERT_TRUE(d.stale);
  TEST_ASSERT_FALSE(d.delta_valid);
  TEST_ASSERT_EQUAL(Comfort::kUnknown, d.comfort);
  TEST_ASSERT_EQUAL_UINT8(0, d.trace_count);
  TEST_ASSERT_FALSE(d.umbrella);
}

void test_derive_data_fully_expired() {
  WeatherModel w = make_weather();
  PressureHistory p;
  const DerivedModel d = derived::derive(w, make_indoor(20, 45), p, kNoon + 30 * kHour, 1800);
  TEST_ASSERT_TRUE(d.stale);
  TEST_ASSERT_EQUAL_UINT8(24, d.first_hour);  // every hour has passed
  TEST_ASSERT_EQUAL_UINT8(0, d.trace_count);
  TEST_ASSERT_FALSE(d.umbrella);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dew_point_magnus);
  RUN_TEST(test_comfort_bands_and_priority);
  RUN_TEST(test_wmo_mapping);
  RUN_TEST(test_condition_text_fits_column);
  RUN_TEST(test_compass8);
  RUN_TEST(test_uv_band);
  RUN_TEST(test_meter_segments);
  RUN_TEST(test_stale);
  RUN_TEST(test_pressure_trend);
  RUN_TEST(test_pressure_needs_three_hours);
  RUN_TEST(test_pressure_mixed_intervals_and_duplicates);
  RUN_TEST(test_pressure_ring_wraps);
  RUN_TEST(test_derive_day_values);
  RUN_TEST(test_derive_night_icon);
  RUN_TEST(test_derive_after_midnight_uses_tomorrow_row);
  RUN_TEST(test_derive_skips_past_hours);
  RUN_TEST(test_derive_umbrella_by_probability);
  RUN_TEST(test_derive_umbrella_by_amount_only);
  RUN_TEST(test_derive_without_data);
  RUN_TEST(test_derive_data_fully_expired);
  return UNITY_END();
}
