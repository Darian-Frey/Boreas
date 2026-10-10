// Plain data shared between modules (ARCHITECTURE.md §data). No behaviour,
// no Arduino dependencies, so host-native tests can use them.
#pragma once

#include <cstdint>

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
