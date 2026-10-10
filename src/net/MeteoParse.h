// Open-Meteo request URL and response parser (F-008, AV-009).
//
// Header-only and free of Arduino calls, so the same code parses a TLS
// stream on the board and a JSON fixture on the host (test/test_meteo).
// The parser fills a temporary model and copies it to `out` only when
// every check passes: a failed parse never touches the last good data.
#pragma once

#include <ArduinoJson.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "data/Models.h"

namespace meteo {

// Field names come from the request in SPEC.md §Open-Meteo request.
// timeformat=unixtime gives epoch seconds; forecast_hours=24 starts the
// hourly arrays at the current hour.
inline bool build_url(char* buf, size_t size, float latitude, float longitude) {
  const int n = snprintf(
      buf, size,
      "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
      "&timezone=Europe%%2FLondon&timeformat=unixtime&forecast_days=2&forecast_hours=24"
      "&wind_speed_unit=kmh"
      "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,"
      "wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure,uv_index"
      "&hourly=temperature_2m,precipitation_probability,precipitation,weather_code"
      "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,"
      "precipitation_probability_max",
      static_cast<double>(latitude), static_cast<double>(longitude));
  return n > 0 && static_cast<size_t>(n) < size;
}

struct ParseResult {
  bool ok = false;
  char error[48] = "";  // short reason for the Diag page / log
};

namespace detail {

// Plausibility bounds (AV-009). A value outside them fails the whole fetch.
constexpr float kTempMinC = -60.0f, kTempMaxC = 60.0f;
constexpr float kWindMaxKmh = 400.0f;
constexpr float kPressureMinHpa = 850.0f, kPressureMaxHpa = 1100.0f;
constexpr float kUvMax = 20.0f;
constexpr float kPrecipMaxMm = 500.0f;
constexpr int64_t kTimeMin = 1704067200;  // 2024-01-01
constexpr int64_t kTimeMax = 4102444800;  // 2100-01-01
constexpr size_t kMinHourly = 12;         // the trace needs 12 h (F-005)

class Checker {
 public:
  explicit Checker(ParseResult& r) : r_(r) {}
  bool failed() const { return failed_; }

  bool number(JsonVariantConst v, const char* name, float lo, float hi, float& out) {
    if (failed_) return false;
    if (!v.is<float>()) return fail("missing", name);
    const float x = v.as<float>();
    if (std::isnan(x) || x < lo || x > hi) return fail("range", name);
    out = x;
    return true;
  }

  // Hourly probabilities and precipitation can be null for some models;
  // treat null as 0 rather than failing the fetch.
  bool nullable(JsonVariantConst v, const char* name, float lo, float hi, float& out) {
    if (v.isNull()) {
      out = 0.0f;
      return true;
    }
    return number(v, name, lo, hi, out);
  }

  bool time(JsonVariantConst v, const char* name, time_t& out) {
    if (failed_) return false;
    if (!v.is<int64_t>()) return fail("missing", name);
    const int64_t t = v.as<int64_t>();
    if (t < kTimeMin || t > kTimeMax) return fail("range", name);
    out = static_cast<time_t>(t);
    return true;
  }

  bool fail(const char* kind, const char* name) {
    if (!failed_) snprintf(r_.error, sizeof(r_.error), "%s: %s", kind, name);
    failed_ = true;
    return false;
  }

 private:
  ParseResult& r_;
  bool failed_ = false;
};

inline void build_filter(JsonDocument& f) {
  for (const char* k : {"time", "temperature_2m", "apparent_temperature", "relative_humidity_2m",
                        "weather_code", "wind_speed_10m", "wind_direction_10m", "wind_gusts_10m",
                        "surface_pressure", "uv_index"})
    f["current"][k] = true;
  for (const char* k : {"time", "temperature_2m", "precipitation_probability", "precipitation", "weather_code"})
    f["hourly"][k] = true;
  for (const char* k : {"time", "temperature_2m_max", "temperature_2m_min", "sunrise", "sunset", "uv_index_max",
                        "precipitation_probability_max"})
    f["daily"][k] = true;
}

}  // namespace detail

// Input: anything ArduinoJson reads (Stream& on the board, const char* in
// tests). fetched_utc is stamped by the caller, not here.
template <typename Input>
ParseResult parse_forecast(Input&& input, WeatherModel& out) {
  ParseResult r;
  JsonDocument filter;
  detail::build_filter(filter);
  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, input, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(4));
  if (err) {
    snprintf(r.error, sizeof(r.error), "json: %s", err.c_str());
    return r;
  }

  using namespace detail;
  Checker c(r);
  WeatherModel m = out;  // keep fields the parser doesn't own (valid, fetched_utc)
  float f = 0.0f;

  // Current conditions
  JsonObjectConst cur = doc["current"];
  CurrentConditions& cc = m.current;
  c.time(cur["time"], "current.time", cc.time);
  c.number(cur["temperature_2m"], "current.temperature_2m", kTempMinC, kTempMaxC, cc.temp_c);
  c.number(cur["apparent_temperature"], "current.apparent_temperature", kTempMinC, kTempMaxC, cc.feels_c);
  if (c.number(cur["relative_humidity_2m"], "current.relative_humidity_2m", 0, 100, f)) cc.rh = static_cast<uint8_t>(f + 0.5f);
  if (c.number(cur["weather_code"], "current.weather_code", 0, 99, f)) cc.weather_code = static_cast<uint8_t>(f);
  c.number(cur["wind_speed_10m"], "current.wind_speed_10m", 0, kWindMaxKmh, cc.wind_kmh);
  if (c.number(cur["wind_direction_10m"], "current.wind_direction_10m", 0, 360, f)) cc.wind_dir_deg = static_cast<uint16_t>(f + 0.5f) % 360;
  c.number(cur["wind_gusts_10m"], "current.wind_gusts_10m", 0, kWindMaxKmh, cc.gust_kmh);
  c.number(cur["surface_pressure"], "current.surface_pressure", kPressureMinHpa, kPressureMaxHpa, cc.pressure_hpa);
  c.number(cur["uv_index"], "current.uv_index", 0, kUvMax, cc.uv);

  // Daily rows: need at least today.
  JsonObjectConst day = doc["daily"];
  JsonArrayConst dtime = day["time"];
  const size_t nd = dtime.size() < static_cast<size_t>(kDailyRows) ? dtime.size() : kDailyRows;
  if (!c.failed() && nd < 1) c.fail("missing", "daily.time");
  for (size_t i = 0; i < nd && !c.failed(); ++i) {
    DailyForecast& d = m.daily[i];
    c.time(dtime[i], "daily.time", d.date);
    c.number(day["temperature_2m_max"][i], "daily.temperature_2m_max", kTempMinC, kTempMaxC, d.max_c);
    c.number(day["temperature_2m_min"][i], "daily.temperature_2m_min", kTempMinC, kTempMaxC, d.min_c);
    c.time(day["sunrise"][i], "daily.sunrise", d.sunrise);
    c.time(day["sunset"][i], "daily.sunset", d.sunset);
    c.number(day["uv_index_max"][i], "daily.uv_index_max", 0, kUvMax, d.uv_max);
    if (c.nullable(day["precipitation_probability_max"][i], "daily.precipitation_probability_max", 0, 100, f))
      d.precip_prob_max = static_cast<uint8_t>(f + 0.5f);
  }

  // Hourly rows: every array the same length, at least 12.
  JsonObjectConst hr = doc["hourly"];
  JsonArrayConst htime = hr["time"];
  const size_t n = htime.size();
  if (!c.failed() && n < kMinHourly) c.fail("short", "hourly.time");
  for (const char* k : {"temperature_2m", "precipitation_probability", "precipitation", "weather_code"})
    if (!c.failed() && hr[k].as<JsonArrayConst>().size() != n) c.fail("length", "hourly arrays");
  const size_t nh = n < static_cast<size_t>(kHourlyRows) ? n : kHourlyRows;
  for (size_t i = 0; i < nh && !c.failed(); ++i) {
    HourlyForecast& h = m.hourly[i];
    c.time(htime[i], "hourly.time", h.time);
    c.number(hr["temperature_2m"][i], "hourly.temperature_2m", kTempMinC, kTempMaxC, h.temp_c);
    if (c.nullable(hr["precipitation_probability"][i], "hourly.precipitation_probability", 0, 100, f))
      h.precip_prob = static_cast<uint8_t>(f + 0.5f);
    c.nullable(hr["precipitation"][i], "hourly.precipitation", 0, kPrecipMaxMm, h.precip_mm);
    if (c.number(hr["weather_code"][i], "hourly.weather_code", 0, 99, f)) h.weather_code = static_cast<uint8_t>(f);
  }

  if (c.failed()) return r;
  m.daily_count = static_cast<uint8_t>(nd);
  m.hourly_count = static_cast<uint8_t>(nh);
  out = m;
  r.ok = true;
  return r;
}

}  // namespace meteo
