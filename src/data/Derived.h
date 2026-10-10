// Values computed from the models for display (SPEC.md §Derived values), so
// ui/ does no arithmetic beyond pixel maths (ARCHITECTURE.md §Data flow).
// Pure: no Arduino calls, no heap. Unit-tested on the host (test/test_derived).
#pragma once

#include <cstdint>
#include <ctime>

#include "data/Models.h"
#include "data/PressureHistory.h"

enum class Comfort : uint8_t { kUnknown, kNominal, kDry, kHumid, kCold, kWarm };

enum class Icon : uint8_t {
  kNone, kSun, kMoon, kSunCloud, kMoonCloud, kCloud, kFog, kDrizzle, kRain, kRainIce, kSnow, kStorm
};

struct DerivedModel {
  // Indoor (F-003)
  float indoor_dew_c = 0.0f;
  Comfort comfort = Comfort::kUnknown;
  uint8_t rh_segments = 0;  // 0-10, humidity level meter

  // Outdoor (F-002, F-004)
  bool weather_valid = false;  // a forecast has been fetched at least once
  bool stale = true;           // older than 2x the fetch interval (F-007)
  bool is_night = false;       // outside today's sunrise-sunset
  uint8_t today = 0;           // index into WeatherModel::daily for the current local day
  const char* condition = "";  // e.g. "LIGHT RAIN"; static string
  Icon icon = Icon::kNone;
  const char* wind_compass = "";  // 8-point, e.g. "SW"
  const char* uv_band = "";       // LOW / MOD / HIGH / V.HIGH / EXTREME
  float outdoor_dew_c = 0.0f;
  bool delta_valid = false;
  float delta_c = 0.0f;  // indoor - outdoor
  PressureHistory::Trend pressure_trend = PressureHistory::Trend::kUnknown;

  // Next 12 h (F-005, F-006)
  uint8_t first_hour = 0;     // index into WeatherModel::hourly of the current hour
  uint8_t trace_count = 0;    // points in the 12 h trace (up to 13: now + 12 h)
  float trace_min_c = 0.0f;
  float trace_max_c = 0.0f;
  uint8_t trace_peak = 0;     // offset from first_hour of the highest point
  uint8_t max_prob_12h = 0;   // %
  float max_precip_12h = 0.0f;  // mm in any one hour
  uint8_t rain_segments = 0;  // 0-12, rain meter
  bool umbrella = false;      // UMBRELLA ADVISED (F-006)
};

namespace derived {

// Magnus formula (SPEC.md §Derived values). rh is clamped to >= 1 %.
float dew_point_c(float temp_c, float rh);

Comfort comfort(float temp_c, float rh);
const char* comfort_text(Comfort c);  // "NOMINAL", "DRY", ...

// WMO weather code -> condition text and day icon (SPEC.md table).
const char* condition_text(uint8_t wmo);
Icon day_icon(uint8_t wmo);
Icon night_variant(Icon day);  // sun -> moon, sun-cloud -> moon-cloud

const char* compass8(uint16_t degrees);
const char* uv_band(float uv);

// Rounds pct (0-100) onto a meter of `segments` cells.
uint8_t meter_segments(float pct, uint8_t segments);

bool is_stale(const WeatherModel& w, time_t now_utc, uint32_t fetch_interval_s);

// Everything at once. `pressure` should already contain the latest sample.
DerivedModel derive(const WeatherModel& weather, const IndoorModel& indoor, const PressureHistory& pressure,
                    time_t now_utc, uint32_t fetch_interval_s);

}  // namespace derived
