#include "display/St7305Panel.h"

#include <Arduino.h>
#include <SPI.h>

#include "pins.h"

namespace {

// SPEC.md §Panel and §Panel timing (measured in Phase 0).
constexpr uint32_t kSpiClockHz = 24000000;
constexpr uint32_t kHpmHoldMs = 100;

constexpr uint8_t kCmdHighPowerMode = 0x38;
constexpr uint8_t kCmdLowPowerMode = 0x39;

// Full-buffer constructor: the 15 000-byte buffer is static inside U8g2,
// so nothing here allocates on the heap (AV-002).
U8G2_ST7305_300X400_F_4W_HW_SPI u8g2(U8G2_R1, pins::kLcdCs, pins::kLcdDc, pins::kLcdRst);

}  // namespace

void St7305Panel::begin() {
  // Must precede u8g2.begin(): the constructor pins alone don't route SPI
  // on this core (CLAUDE.md §Pitfalls).
  SPI.begin(pins::kLcdSclk, -1, pins::kLcdMosi, pins::kLcdCs);
  u8g2.setBusClock(kSpiClockHz);
  // The driver's init sequence clears panel RAM and leaves it in HPM.
  u8g2.begin();
  u8g2.clearBuffer();
  end_frame();
}

uint32_t St7305Panel::reinit() {
  const uint32_t t0 = millis();
  // The same start-up as begin(): U8g2 pulses RST and sends the driver's
  // init sequence. SPI pins and clock are already configured.
  u8g2.begin();
  return millis() - t0;
}

U8G2& St7305Panel::canvas() { return u8g2; }

u8g2_t* St7305Panel::raw() { return u8g2.getU8g2(); }

void St7305Panel::end_frame() {
  u8g2.sendF("c", kCmdHighPowerMode);
  uint32_t t0 = micros();
  u8g2.sendBuffer();
  last_send_us_ = micros() - t0;
  delay(kHpmHoldMs);
  u8g2.sendF("c", kCmdLowPowerMode);
}
