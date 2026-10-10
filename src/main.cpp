// Boreas MS-42 firmware entry point.
//
// Temporary Phase 1 scaffold: proves display/ and sensors/ on the board by
// reading the sensors and redrawing a placeholder card once a minute. The
// drawing here moves to ui/ and the timing to app/Scheduler as those
// modules arrive.

#include <Arduino.h>
#include <Wire.h>

#include "data/Models.h"
#include "display/St7305Panel.h"
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
  c.drawStr(12, 50, "PHASE 1 - DISPLAY + SENSORS");
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

  panel.begin();
  redraw();
  last_redraw_ms = millis();
}

void loop() {
  if (millis() - last_redraw_ms >= kRedrawIntervalMs) {
    last_redraw_ms += kRedrawIntervalMs;
    redraw();
  }
  delay(10);
}
