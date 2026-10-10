// Boreas MS-42 firmware entry point.
//
// Temporary Phase 1 scaffold: proves display/, sensors/ and net/ on the
// board. It reads the sensors, keeps Wi-Fi and NTP running, and redraws a
// placeholder card every minute and on any state change. The drawing here
// moves to ui/ and the timing to app/Scheduler as those modules arrive.

#include <Arduino.h>
#include <Wire.h>
#include <sys/time.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h: copy include/secrets.example.h and add Wi-Fi credentials (D-010)"
#endif

#include "data/Models.h"
#include "display/St7305Panel.h"
#include "net/NtpSync.h"
#include "net/WiFiMgr.h"
#include "pins.h"
#include "sensors/Battery.h"
#include "sensors/CivilTime.h"
#include "sensors/Rtc.h"
#include "sensors/Shtc3.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kRedrawIntervalMs = 60000;  // SPEC.md §Scheduling

St7305Panel panel;
Shtc3 shtc3(Wire);
Rtc rtc(Wire);
Battery battery;
WiFiMgr wifi;
NtpSync ntp(rtc);
bool clock_valid = false;  // system clock set from a valid RTC or NTP

IndoorModel indoor;
BatteryModel batt;
RtcReading clock_reading;
uint32_t last_redraw_ms = 0;
uint32_t redraw_count = 0;

void draw_placeholder() {
  U8G2& c = panel.canvas();
  char line[48];
  c.clearBuffer();
  c.drawFrame(3, 3, 394, 294);
  c.drawFrame(5, 5, 390, 290);
  c.drawBox(5, 5, 390, 20);
  c.setDrawColor(0);
  c.setFont(u8g2_font_7x14B_tr);
  c.drawStr(10, 20, "BOREAS MS-42");
  c.setDrawColor(1);
  c.setFont(u8g2_font_6x12_tr);
  c.drawStr(12, 50, "PHASE 1 - DISPLAY + SENSORS + NET");
  snprintf(line, sizeof(line), "REDRAW %lu  UPTIME %lu MIN", static_cast<unsigned long>(redraw_count),
           static_cast<unsigned long>(millis() / 60000));
  c.drawStr(12, 66, line);
  snprintf(line, sizeof(line), "LAST SEND %lu MS", static_cast<unsigned long>(panel.last_send_us() / 1000));
  c.drawStr(12, 82, line);

  if (indoor.valid)
    snprintf(line, sizeof(line), "INDOOR %.1f C  %.0f %%RH  (RAW %.1f)", indoor.temp_c, indoor.rh,
             indoor.raw_temp_c);
  else
    snprintf(line, sizeof(line), "INDOOR SENSOR FAIL");
  c.drawStr(12, 106, line);

  snprintf(line, sizeof(line), "BATT %.2f V  %u/4%s%s", batt.volts, batt.cells, batt.low ? "  LOW" : "",
           batt.halt ? "  HALT" : "");
  c.drawStr(12, 122, line);

  if (!clock_reading.present) {
    snprintf(line, sizeof(line), "RTC MISSING");
  } else if (!clock_reading.valid) {
    snprintf(line, sizeof(line), "RTC UNSET  --:--  (WAITING FOR NTP)");
  } else {
    civil::DateTime t = civil::from_utc(clock_reading.utc);
    snprintf(line, sizeof(line), "RTC OK  %04d-%02d-%02d %02d:%02d UTC", t.year, t.month, t.day, t.hour,
             t.minute);
  }
  c.drawStr(12, 138, line);

  const uint32_t now_ms = millis();
  switch (wifi.state()) {
    case WiFiMgr::State::kConnected:
      snprintf(line, sizeof(line), "WIFI OK  %d DBM  RECONNECTS %lu", wifi.rssi(),
               static_cast<unsigned long>(wifi.reconnects()));
      break;
    case WiFiMgr::State::kConnecting:
      snprintf(line, sizeof(line), "WIFI CONNECTING");
      break;
    case WiFiMgr::State::kWaiting:
      snprintf(line, sizeof(line), "WIFI DOWN  RETRY IN %lu S",
               static_cast<unsigned long>(wifi.retry_in_ms(now_ms) / 1000));
      break;
    case WiFiMgr::State::kOff:
      snprintf(line, sizeof(line), "WIFI OFF");
      break;
  }
  c.drawStr(12, 162, line);

  if (ntp.synced())
    snprintf(line, sizeof(line), "NTP OK  SYNCS %lu  RTC DRIFT %ld S", static_cast<unsigned long>(ntp.sync_count()),
             static_cast<long>(ntp.last_drift_s()));
  else
    snprintf(line, sizeof(line), "NTP %s", ntp.first_sync_overdue(now_ms) ? "OVERDUE" : "WAITING");
  c.drawStr(12, 178, line);

  if (clock_valid) {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    strftime(line, sizeof(line), "LOCAL %a %d %b %H:%M %Z", &local);
  } else {
    snprintf(line, sizeof(line), "LOCAL --:--");
  }
  c.drawStr(12, 194, line);
}

void read_sensors() {
  indoor = shtc3.read();
  batt = battery.read();
  clock_reading = rtc.read();
  Serial.printf("indoor valid=%d T=%.2f RH=%.1f | batt %.2f V cells=%u low=%d halt=%d | rtc present=%d valid=%d\n",
                indoor.valid, indoor.temp_c, indoor.rh, batt.volts, batt.cells, batt.low, batt.halt,
                clock_reading.present, clock_reading.valid);
}

void redraw() {
  ++redraw_count;
  read_sensors();
  draw_placeholder();
  panel.end_frame();
  Serial.printf("redraw %lu: send %lu us\n", static_cast<unsigned long>(redraw_count),
                static_cast<unsigned long>(panel.last_send_us()));
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  // An unread USB CDC port must drop output, not stall loop() (AV-011).
  Serial.setTxTimeoutMs(0);
  Serial.println("\nBoreas MS-42");

  Wire.begin(pins::kI2cSda, pins::kI2cScl);
  battery.begin();
  Serial.printf("SHTC3 %s\n", shtc3.begin() ? "OK" : "NOT FOUND");

  // D-007: show a correct clock before Wi-Fi, but only from a trusted RTC (D-015).
  ntp.begin();
  RtcReading boot_rtc = rtc.read();
  if (boot_rtc.valid) {
    struct timeval tv = {boot_rtc.utc, 0};
    settimeofday(&tv, nullptr);
    clock_valid = true;
  }
  Serial.printf("RTC at boot: %s\n", boot_rtc.valid ? "valid, system clock set" : "unset, waiting for NTP");

  wifi.begin(secrets::kWifiSsid, secrets::kWifiPassword);

  panel.begin();
  redraw();
  last_redraw_ms = millis();
}

void loop() {
  const uint32_t now_ms = millis();
  const WiFiMgr::State wifi_before = wifi.state();
  const uint32_t syncs_before = ntp.sync_count();

  wifi.service(now_ms);
  ntp.service(now_ms, wifi.connected());
  if (ntp.synced()) clock_valid = true;

  if (wifi.state() != wifi_before) {
    Serial.printf("wifi state %d -> %d\n", static_cast<int>(wifi_before), static_cast<int>(wifi.state()));
  }
  // Redraw on the minute, and immediately on any data change (SPEC.md §Scheduling).
  const bool changed = wifi.connected() != (wifi_before == WiFiMgr::State::kConnected) ||
                       ntp.sync_count() != syncs_before;
  if (changed || now_ms - last_redraw_ms >= kRedrawIntervalMs) {
    last_redraw_ms = now_ms;
    redraw();
  }
  delay(10);
}
