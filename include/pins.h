// Every GPIO used by Boreas MS-42. No numeric pin literals elsewhere.
// Source: Waveshare ESP32-S3-RLCD-4.2 schematic; see SPEC.md §Pin map.
#pragma once

#include <cstdint>

namespace pins {

// ST7305 RLCD (SPI). SPI.begin(kLcdSclk, -1, kLcdMosi, kLcdCs) must
// precede u8g2.begin().
constexpr int8_t kLcdSclk = 11;
constexpr int8_t kLcdMosi = 12;
constexpr int8_t kLcdDc = 5;
constexpr int8_t kLcdCs = 40;
constexpr int8_t kLcdRst = 41;
constexpr int8_t kLcdTe = 6;  // tearing-effect output; unused

// Shared I2C bus: PCF85063 RTC (0x51), SHTC3 (0x70). Also on header P1-14/16.
constexpr int8_t kI2cSda = 13;
constexpr int8_t kI2cScl = 14;
constexpr int8_t kRtcInt = 15;  // unused

// Buttons, active low.
constexpr int8_t kBootButton = 0;
constexpr int8_t kKeyButton = 18;
constexpr int8_t kLocationButton = 17;  // add-on switch to GND on header P1-13

// TF (microSD) slot. The schematic labels these MOSI / SCK / MISO with no
// CS line, i.e. the SDMMC peripheral in 1-bit mode: CMD, CLK, D0.
constexpr int8_t kSdCmd = 21;
constexpr int8_t kSdClk = 38;
constexpr int8_t kSdD0 = 39;

// Battery voltage through a 3:1 divider [verify ratio].
constexpr int8_t kBatteryAdc = 4;

// Audio (unused). The speaker amp enable must be held LOW.
constexpr int8_t kSpeakerAmpEnable = 46;

}  // namespace pins
