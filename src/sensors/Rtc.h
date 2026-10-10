// NXP PCF85063 real-time clock on the shared I2C bus (0x51). Holds UTC.
// Validity rule (D-015): the time is trusted only when the oscillator-stop
// flag is clear AND the RAM byte holds config::kRtcNtpMarker.
#pragma once

#include <Wire.h>

#include <ctime>

struct RtcReading {
  bool present = false;  // chip answered on I2C
  bool valid = false;    // D-015: OS flag clear and NTP marker present
  time_t utc = 0;        // meaningful only when valid
};

class Rtc {
 public:
  explicit Rtc(TwoWire& wire) : wire_(wire) {}

  RtcReading read();

  // Writes the time and then the NTP marker. Only NtpSync may call this
  // (D-015): nothing else may mark the RTC as trusted.
  bool set_from_ntp(time_t utc);

 private:
  TwoWire& wire_;
};
