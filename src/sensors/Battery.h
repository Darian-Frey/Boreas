// 18650 cell voltage via the 3:1 divider on GPIO4 (SPEC.md §Battery).
#pragma once

#include "config.h"
#include "data/Models.h"

class Battery {
 public:
  // Sets the ADC attenuation (11 dB). Call once at boot.
  void begin();

  // Oversampled reading with gauge cells and LOW/HALT flags.
  BatteryModel read();

  // Pure mapping from volts to the model; header-only so host tests can use it.
  static BatteryModel from_volts(float volts) {
    BatteryModel m;
    m.valid = true;
    m.volts = volts;
    for (float threshold : config::kBatteryCellThresholdsV)
      if (volts >= threshold) ++m.cells;
    m.low = volts < config::kBatteryLowV;
    m.halt = volts < config::kBatteryHaltV;
    return m;
  }
};
