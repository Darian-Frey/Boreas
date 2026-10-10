#include "sensors/Rtc.h"

#include "config.h"
#include "sensors/CivilTime.h"

namespace {

constexpr uint8_t kAddr = 0x51;
constexpr uint8_t kRegRam = 0x03;      // free RAM byte: NTP marker (SPEC.md §RTC)
constexpr uint8_t kRegSeconds = 0x04;  // then minutes, hours, day, weekday, month, year
constexpr uint8_t kOscStopFlag = 0x80;

uint8_t from_bcd(uint8_t v) { return static_cast<uint8_t>((v >> 4) * 10 + (v & 0x0F)); }
uint8_t to_bcd(int v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }

}  // namespace

RtcReading Rtc::read() {
  RtcReading r;
  // RAM byte and the seven time registers in one burst, so the time is
  // consistent (the chip freezes its counters during the access).
  wire_.beginTransmission(kAddr);
  wire_.write(kRegRam);
  if (wire_.endTransmission(false) != 0) return r;
  if (wire_.requestFrom(kAddr, static_cast<uint8_t>(8)) != 8) return r;
  uint8_t reg[8];
  for (uint8_t& b : reg) b = wire_.read();
  r.present = true;

  const uint8_t ram = reg[0];
  const uint8_t sec = reg[1];
  civil::DateTime t{2000 + from_bcd(reg[7]),       from_bcd(reg[6] & 0x1F), from_bcd(reg[4] & 0x3F),
                    from_bcd(reg[3] & 0x3F),       from_bcd(reg[2] & 0x7F), from_bcd(sec & 0x7F)};
  const bool in_range = t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31 && t.hour <= 23 &&
                        t.minute <= 59 && t.second <= 59;

  r.valid = !(sec & kOscStopFlag) && ram == config::kRtcNtpMarker && in_range;
  if (in_range) r.utc = civil::to_utc(t);
  return r;
}

bool Rtc::set_from_ntp(time_t utc) {
  const civil::DateTime t = civil::from_utc(utc);
  if (t.year < 2000 || t.year > 2099) return false;

  // Time first (writing seconds clears the OS flag), marker last, so a
  // failed write never leaves the marker beside a wrong time.
  wire_.beginTransmission(kAddr);
  wire_.write(kRegSeconds);
  wire_.write(to_bcd(t.second));
  wire_.write(to_bcd(t.minute));
  wire_.write(to_bcd(t.hour));
  wire_.write(to_bcd(t.day));
  wire_.write(static_cast<uint8_t>(civil::weekday(utc)));
  wire_.write(to_bcd(t.month));
  wire_.write(to_bcd(t.year - 2000));
  if (wire_.endTransmission() != 0) return false;

  wire_.beginTransmission(kAddr);
  wire_.write(kRegRam);
  wire_.write(config::kRtcNtpMarker);
  return wire_.endTransmission() == 0;
}
