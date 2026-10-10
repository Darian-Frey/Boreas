#include "sensors/Battery.h"

#include <Arduino.h>

#include "config.h"
#include "pins.h"

void Battery::begin() {
  // Global form: the per-pin call errors until the pin is in analogue mode.
  analogSetAttenuation(ADC_11db);
}

BatteryModel Battery::read() {
  uint32_t sum_mv = 0;
  for (int i = 0; i < config::kBatterySamples; ++i) sum_mv += analogReadMilliVolts(pins::kBatteryAdc);
  const float pin_mv = static_cast<float>(sum_mv) / config::kBatterySamples;
  return from_volts(pin_mv * config::kBatteryDivider / 1000.0f);
}
