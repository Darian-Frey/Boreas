// Boreas MS-42 firmware entry point.
//
// Temporary Phase 1 scaffold: proves display/St7305Panel on the board by
// redrawing a placeholder card once a minute. The drawing here moves to
// ui/ and the timing to app/Scheduler as those modules arrive.

#include <Arduino.h>

#include "display/St7305Panel.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kRedrawIntervalMs = 60000;  // SPEC.md §Scheduling

St7305Panel panel;
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
  c.drawStr(12, 50, "PHASE 1 - DISPLAY MODULE");
  snprintf(line, sizeof(line), "REDRAW %lu  UPTIME %lu MIN", static_cast<unsigned long>(redraw_count),
           static_cast<unsigned long>(millis() / 60000));
  c.drawStr(12, 66, line);
  snprintf(line, sizeof(line), "LAST SEND %lu MS", static_cast<unsigned long>(panel.last_send_us() / 1000));
  c.drawStr(12, 82, line);
}

void redraw() {
  ++redraw_count;
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
