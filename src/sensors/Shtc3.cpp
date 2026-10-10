#include "sensors/Shtc3.h"

#include <Arduino.h>

#include "config.h"

namespace {

constexpr uint8_t kAddr = 0x70;
constexpr uint16_t kCmdWake = 0x3517;
constexpr uint16_t kCmdSleep = 0xB098;
constexpr uint16_t kCmdReadId = 0xEFC8;
constexpr uint16_t kCmdMeasureTFirst = 0x7866;  // normal mode, no clock stretching
constexpr uint32_t kWakeDelayUs = 300;          // datasheet max 240 us
constexpr uint32_t kMeasureDelayMs = 15;        // datasheet max 12.1 ms
constexpr uint16_t kIdMask = 0x083F;
constexpr uint16_t kIdValue = 0x0807;

uint8_t crc8(uint16_t word) {
  uint8_t crc = 0xFF;
  for (uint8_t byte : {static_cast<uint8_t>(word >> 8), static_cast<uint8_t>(word & 0xFF)}) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
  }
  return crc;
}

}  // namespace

bool Shtc3::command(uint16_t cmd) {
  wire_.beginTransmission(kAddr);
  wire_.write(cmd >> 8);
  wire_.write(cmd & 0xFF);
  return wire_.endTransmission() == 0;
}

// Reads `count` 16-bit words, each followed by a CRC byte.
bool Shtc3::read_words(uint16_t* out, int count) {
  const uint8_t bytes = static_cast<uint8_t>(count * 3);
  if (wire_.requestFrom(kAddr, bytes) != bytes) return false;
  for (int i = 0; i < count; ++i) {
    uint16_t word = static_cast<uint16_t>(wire_.read() << 8);
    word |= wire_.read();
    if (crc8(word) != wire_.read()) return false;
    out[i] = word;
  }
  return true;
}

bool Shtc3::begin() {
  uint16_t id = 0;
  bool ok = command(kCmdWake);
  delayMicroseconds(kWakeDelayUs);
  ok = ok && command(kCmdReadId) && read_words(&id, 1);
  command(kCmdSleep);
  return ok && (id & kIdMask) == kIdValue;
}

IndoorModel Shtc3::read() {
  IndoorModel m;
  uint16_t raw[2] = {0, 0};
  if (!command(kCmdWake)) return m;
  delayMicroseconds(kWakeDelayUs);
  bool ok = command(kCmdMeasureTFirst);
  if (ok) {
    delay(kMeasureDelayMs);
    ok = read_words(raw, 2);
  }
  command(kCmdSleep);
  if (!ok) return m;

  m.raw_temp_c = -45.0f + 175.0f * raw[0] / 65536.0f;
  m.rh = 100.0f * raw[1] / 65536.0f;
  m.temp_c = m.raw_temp_c + config::kIndoorTempOffsetC;
  m.valid = true;
  return m;
}
