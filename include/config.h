// Tunable constants. SPEC.md is the source of truth for every value here;
// change both together.
#pragma once

#include <cstdint>

namespace config {

// ---- Battery (SPEC.md §Battery)
constexpr float kBatteryDivider = 3.0f;  // [verify against a multimeter]
constexpr int kBatterySamples = 16;
// Gauge: cells lit when V_batt >= threshold, lowest first (0 cells below 3.4 V).
constexpr float kBatteryCellThresholdsV[] = {3.4f, 3.6f, 3.8f, 4.0f};
constexpr float kBatteryLowV = 3.4f;   // footer LOW BATT below this
constexpr float kBatteryHaltV = 3.2f;  // final redraw then deep sleep below this

// ---- Indoor sensor (AV-004)
// Added to the SHTC3 temperature to cancel board heat. 0 until a stable
// offset is measured; the raw value is always kept for the Diag page.
constexpr float kIndoorTempOffsetC = 0.0f;

// ---- RTC (SPEC.md §RTC, D-015)
// Value in the PCF85063 RAM byte meaning "time was set by NTP".
constexpr uint8_t kRtcNtpMarker = 0xB5;

}  // namespace config
