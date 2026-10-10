// Sensirion SHTC3 indoor temperature/humidity sensor on the shared I2C bus
// (0x70). One-shot reads: wake, measure (normal mode), sleep (SPEC.md
// §Scheduling).
#pragma once

#include <Wire.h>

#include "data/Models.h"

class Shtc3 {
 public:
  explicit Shtc3(TwoWire& wire) : wire_(wire) {}

  // Reads the ID register; true if an SHTC3 answers with a valid ID.
  bool begin();

  // One measurement. valid=false on any I2C or CRC failure. The calibration
  // offset (config::kIndoorTempOffsetC) is applied to temp_c only.
  IndoorModel read();

 private:
  bool command(uint16_t cmd);
  bool read_words(uint16_t* out, int count);

  TwoWire& wire_;
};
