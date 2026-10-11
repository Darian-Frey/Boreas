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

// ---- Wi-Fi (F-010, SPEC.md §Scheduling)
constexpr const char* kHostname = "boreas-ms42";
constexpr uint32_t kWifiConnectTimeoutMs = 15000;   // one association attempt
constexpr uint32_t kWifiBackoffInitialMs = 5000;    // first retry after a failure
constexpr uint32_t kWifiBackoffCapMs = 300000;      // retries never spaced wider than 5 min

// ---- Time (D-007, F-009)
// Europe/London with BST. Applies only to local-time display; the system
// clock and the RTC hold UTC.
constexpr const char* kTimezone = "GMT0BST,M3.5.0/1,M10.5.0";
constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "time.google.com";
constexpr uint32_t kNtpIntervalMs = 6UL * 60 * 60 * 1000;  // re-sync and RTC write every 6 h
constexpr uint32_t kNtpFirstSyncTimeoutMs = 60000;        // after Wi-Fi up, flag failure if no sync

// ---- Forecast fetch (F-008, SPEC.md §Scheduling and §Retry)
// Schedule in clock seconds (app/Schedule): fetches land on aligned slots,
// 15 s past :00/:15/:30/:45 as the interval allows.
constexpr uint32_t kFetchIntervalS = 30 * 60;
constexpr uint32_t kFetchAdvisoryIntervalS = 15 * 60;  // while UMBRELLA ADVISED
constexpr uint32_t kFetchNightIntervalS = 60 * 60;     // night mode (F-014); wins over advisory
constexpr uint32_t kFetchSlotOffsetS = 15;
constexpr uint32_t kFetchRetryInitialS = 60;
constexpr uint32_t kFetchRetryCapS = 15 * 60;
constexpr uint32_t kHttpConnectTimeoutMs = 10000;
constexpr uint16_t kHttpReadTimeoutMs = 10000;
constexpr uint32_t kTlsHandshakeTimeoutS = 10;

// ---- Health log on the TF card (SPEC.md §Health log, AV-002)
constexpr const char* kHealthLogPath = "/boreas/health.csv";
constexpr uint32_t kHealthLogIntervalS = 10 * 60;

}  // namespace config
