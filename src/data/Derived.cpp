#include "data/Derived.h"

#include <cmath>

namespace derived {

namespace {

// SPEC.md §Derived values and F-003 / F-006.
constexpr float kMagnusB = 17.62f;
constexpr float kMagnusC = 243.12f;
constexpr float kComfortRhMin = 30.0f, kComfortRhMax = 60.0f;
constexpr float kComfortTMinC = 18.0f, kComfortTMaxC = 24.0f;
constexpr uint8_t kUmbrellaProbPct = 50;
constexpr float kUmbrellaPrecipMm = 0.5f;
constexpr int kOutlookHours = 12;
constexpr uint8_t kRhSegments = 10;
constexpr uint8_t kRainSegments = 12;

}  // namespace

float dew_point_c(float temp_c, float rh) {
  const float r = rh < 1.0f ? 1.0f : rh;
  const float g = std::log(r / 100.0f) + kMagnusB * temp_c / (kMagnusC + temp_c);
  return kMagnusC * g / (kMagnusB - g);
}

// RH wins over temperature when both are out of band (SPEC.md).
Comfort comfort(float temp_c, float rh) {
  if (rh < kComfortRhMin) return Comfort::kDry;
  if (rh > kComfortRhMax) return Comfort::kHumid;
  if (temp_c < kComfortTMinC) return Comfort::kCold;
  if (temp_c > kComfortTMaxC) return Comfort::kWarm;
  return Comfort::kNominal;
}

const char* comfort_text(Comfort c) {
  switch (c) {
    case Comfort::kNominal: return "NOMINAL";
    case Comfort::kDry: return "DRY";
    case Comfort::kHumid: return "HUMID";
    case Comfort::kCold: return "COLD";
    case Comfort::kWarm: return "WARM";
    case Comfort::kUnknown: break;
  }
  return "---";
}

const char* condition_text(uint8_t wmo) {
  switch (wmo) {
    case 0: return "CLEAR";
    case 1: return "MAINLY CLEAR";
    case 2: return "PARTLY CLOUDY";
    case 3: return "OVERCAST";
    case 45: case 48: return "FOG";
    case 51: case 53: case 55: case 56: case 57: return "DRIZZLE";
    case 61: case 80: return "LIGHT RAIN";
    case 63: case 81: return "MODERATE RAIN";
    case 65: case 82: return "HEAVY RAIN";
    case 66: case 67: return "FREEZING RAIN";
    case 71: case 73: case 75: case 77: case 85: case 86: return "SNOW";
    case 95: case 96: case 99: return "THUNDERSTORM";
    default: return "---";
  }
}

Icon day_icon(uint8_t wmo) {
  switch (wmo) {
    case 0: return Icon::kSun;
    case 1: case 2: return Icon::kSunCloud;
    case 3: return Icon::kCloud;
    case 45: case 48: return Icon::kFog;
    case 51: case 53: case 55: case 56: case 57: return Icon::kDrizzle;
    case 61: case 63: case 65: case 80: case 81: case 82: return Icon::kRain;
    case 66: case 67: return Icon::kRainIce;
    case 71: case 73: case 75: case 77: case 85: case 86: return Icon::kSnow;
    case 95: case 96: case 99: return Icon::kStorm;
    default: return Icon::kNone;
  }
}

Icon night_variant(Icon day) {
  if (day == Icon::kSun) return Icon::kMoon;
  if (day == Icon::kSunCloud) return Icon::kMoonCloud;
  return day;
}

const char* compass8(uint16_t degrees) {
  static const char* const kPoints[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return kPoints[((degrees % 360) * 2 + 45) / 90 % 8];
}

// WHO UV index bands, shortened to fit the readouts column.
const char* uv_band(float uv) {
  const int u = static_cast<int>(std::lround(uv));
  if (u <= 2) return "LOW";
  if (u <= 5) return "MOD";
  if (u <= 7) return "HIGH";
  if (u <= 10) return "V.HIGH";
  return "EXTREME";
}

uint8_t meter_segments(float pct, uint8_t segments) {
  if (pct <= 0.0f) return 0;
  if (pct >= 100.0f) return segments;
  return static_cast<uint8_t>(std::lround(pct * segments / 100.0f));
}

uint8_t wifi_bars(int8_t rssi_dbm) {
  if (rssi_dbm >= 0) return 0;  // 0 means "no reading"
  if (rssi_dbm >= -55) return 4;
  if (rssi_dbm >= -67) return 3;
  if (rssi_dbm >= -75) return 2;
  if (rssi_dbm >= -85) return 1;
  return 0;
}

bool is_stale(const WeatherModel& w, time_t now_utc, uint32_t fetch_interval_s) {
  if (!w.valid) return true;
  return now_utc - w.fetched_utc > static_cast<time_t>(2) * fetch_interval_s;
}

DerivedModel derive(const WeatherModel& w, const IndoorModel& in, const PressureHistory& pressure, time_t now_utc,
                    uint32_t fetch_interval_s) {
  DerivedModel d;

  if (in.valid) {
    d.indoor_dew_c = dew_point_c(in.temp_c, in.rh);
    d.comfort = comfort(in.temp_c, in.rh);
    d.rh_segments = meter_segments(in.rh, kRhSegments);
  }

  d.weather_valid = w.valid;
  d.stale = is_stale(w, now_utc, fetch_interval_s);
  d.pressure_trend = pressure.trend();
  if (!w.valid) return d;

  const CurrentConditions& c = w.current;
  d.outdoor_dew_c = dew_point_c(c.temp_c, c.rh);
  d.wind_compass = compass8(c.wind_dir_deg);
  d.uv_band = uv_band(c.uv);
  d.condition = condition_text(c.weather_code);
  if (in.valid) {
    d.delta_valid = true;
    d.delta_c = in.temp_c - c.temp_c;
  }

  // Today = the last daily row starting at or before now (local midnight).
  int today = 0;
  for (int i = 0; i < w.daily_count; ++i)
    if (w.daily[i].date <= now_utc) today = i;
  d.today = static_cast<uint8_t>(today);
  const DailyForecast& day = w.daily[today];
  d.is_night = now_utc < day.sunrise || now_utc >= day.sunset;
  const Icon icon = day_icon(c.weather_code);
  d.icon = d.is_night ? night_variant(icon) : icon;

  // Current hour: first row whose hour hasn't ended. Data may be up to
  // 2x the fetch interval old, so hourly[0] can already be in the past.
  int first = 0;
  while (first < w.hourly_count && w.hourly[first].time + 3600 <= now_utc) ++first;
  d.first_hour = static_cast<uint8_t>(first);

  const int remaining = w.hourly_count - first;
  const int outlook = remaining < kOutlookHours ? remaining : kOutlookHours;
  for (int i = 0; i < outlook; ++i) {
    const HourlyForecast& h = w.hourly[first + i];
    if (h.precip_prob > d.max_prob_12h) d.max_prob_12h = h.precip_prob;
    if (h.precip_mm > d.max_precip_12h) d.max_precip_12h = h.precip_mm;
  }
  d.rain_segments = meter_segments(d.max_prob_12h, kRainSegments);
  d.umbrella = outlook > 0 && (d.max_prob_12h >= kUmbrellaProbPct || d.max_precip_12h >= kUmbrellaPrecipMm);

  // Trace: now plus 12 h, so up to 13 points.
  const int points = remaining < kOutlookHours + 1 ? remaining : kOutlookHours + 1;
  d.trace_count = static_cast<uint8_t>(points > 0 ? points : 0);
  for (int i = 0; i < points; ++i) {
    const float t = w.hourly[first + i].temp_c;
    if (i == 0 || t < d.trace_min_c) d.trace_min_c = t;
    if (i == 0 || t > d.trace_max_c) {
      d.trace_max_c = t;
      d.trace_peak = static_cast<uint8_t>(i);
    }
  }
  return d;
}

}  // namespace derived
