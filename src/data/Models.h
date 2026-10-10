// Plain data shared between modules (ARCHITECTURE.md §data). No behaviour,
// no Arduino dependencies, so host-native tests can use them.
#pragma once

#include <cstdint>
#include <ctime>

// Indoor conditions from the onboard SHTC3 (F-003).
struct IndoorModel {
  bool valid = false;
  float temp_c = 0.0f;      // corrected: raw + config::kIndoorTempOffsetC (AV-004)
  float rh = 0.0f;          // % relative humidity
  float raw_temp_c = 0.0f;  // as measured, for the Diag page
};

// Battery state from the GPIO4 divider (SPEC.md §Battery).
struct BatteryModel {
  bool valid = false;
  float volts = 0.0f;
  uint8_t cells = 0;  // 0-4 for the footer gauge
  bool low = false;   // below config::kBatteryLowV: footer LOW BATT
  bool halt = false;  // below config::kBatteryHaltV: final redraw, then sleep
};

// ---- Forecast from Open-Meteo (F-008, SPEC.md §Open-Meteo request).
// All times are UTC epoch seconds; daily dates are local midnight.

constexpr int kHourlyRows = 24;  // next 24 h from the current hour
constexpr int kDailyRows = 2;    // today, tomorrow (night mode outlook, F-014)

struct CurrentConditions {
  time_t time = 0;
  float temp_c = 0.0f;
  float feels_c = 0.0f;
  uint8_t rh = 0;
  uint8_t weather_code = 0;  // WMO
  float wind_kmh = 0.0f;
  uint16_t wind_dir_deg = 0;
  float gust_kmh = 0.0f;
  float pressure_hpa = 0.0f;  // surface pressure
  float uv = 0.0f;
};

struct DailyForecast {
  time_t date = 0;
  float max_c = 0.0f;
  float min_c = 0.0f;
  time_t sunrise = 0;
  time_t sunset = 0;
  float uv_max = 0.0f;
  uint8_t precip_prob_max = 0;
};

struct HourlyForecast {
  time_t time = 0;
  float temp_c = 0.0f;
  float precip_mm = 0.0f;
  uint8_t precip_prob = 0;
  uint8_t weather_code = 0;
};

struct WeatherModel {
  bool valid = false;        // at least one fetch has succeeded
  time_t fetched_utc = 0;    // when the last successful fetch completed
  CurrentConditions current;
  DailyForecast daily[kDailyRows];
  uint8_t daily_count = 0;
  HourlyForecast hourly[kHourlyRows];
  uint8_t hourly_count = 0;
};
