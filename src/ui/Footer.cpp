#include "ui/Footer.h"

#include <cstdio>
#include <ctime>

#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/grid.h"

namespace ui {

namespace {

using namespace grid;

// Footer region: x 6..393, y 280..293.
constexpr int16_t kLeft = kFooter.x + kPad;                               // 9
constexpr int16_t kRight = static_cast<int16_t>(kFooter.right() - kPad);  // 390
constexpr int16_t kCellW = 4, kCellH = 8, kCellGap = 1;
constexpr int16_t kGaugeW = 4 * kCellW + 3 * kCellGap + 4;  // cells + 2 px padding each side
constexpr int16_t kGaugeH = kCellH + 4;
constexpr int16_t kGap = 6;  // label to gauge, gauge to voltage
constexpr int16_t kGroupGap = 10;

void hhmm(char* buf, size_t size, bool valid, time_t utc) {
  if (!valid || utc == 0) {
    snprintf(buf, size, "--:--");
    return;
  }
  struct tm t;
  localtime_r(&utc, &t);
  snprintf(buf, size, "%02d:%02d", t.tm_hour, t.tm_min);
}

// Four cells filled up to `filled`, right edge at `right`; returns left x.
int16_t draw_cells(u8g2_t* g, int16_t right, int16_t top, uint8_t filled) {
  const int16_t x0 = static_cast<int16_t>(right - (4 * kCellW + 3 * kCellGap) + 1);
  for (int i = 0; i < 4; ++i) {
    const int16_t x = static_cast<int16_t>(x0 + i * (kCellW + kCellGap));
    if (i < filled)
      u8g2_DrawBox(g, x, top, kCellW, kCellH);
    else
      u8g2_DrawFrame(g, x, top, kCellW, kCellH);
  }
  return x0;
}

// Right-hand group: WIFI [bars]  BATT [gauge] 3.9V. Returns its left x.
int16_t draw_status_group(u8g2_t* g, const PanelState& s, int16_t baseline) {
  const int16_t mid = static_cast<int16_t>(kFooter.y + kFooter.h / 2);
  char volts[16];
  if (!s.battery.valid)
    snprintf(volts, sizeof(volts), "--V");
  else if (s.battery.low)
    snprintf(volts, sizeof(volts), "LOW %.1fV", static_cast<double>(s.battery.volts));
  else
    snprintf(volts, sizeof(volts), "%.1fV", static_cast<double>(s.battery.volts));
  int16_t x = draw_right(g, kRight, baseline, volts);

  // Battery: outline with a nub on the right, four cells inside.
  const int16_t body_right = static_cast<int16_t>(x - kGap - 3);
  const int16_t body_left = static_cast<int16_t>(body_right - kGaugeW + 1);
  u8g2_DrawFrame(g, body_left, mid - kGaugeH / 2, kGaugeW, kGaugeH);
  u8g2_DrawBox(g, body_right + 1, mid - 2, 2, 4);
  draw_cells(g, static_cast<int16_t>(body_right - 2), static_cast<int16_t>(mid - kCellH / 2),
             s.battery.valid ? s.battery.cells : 0);
  x = draw_right(g, static_cast<int16_t>(body_left - kGap), baseline, "BATT");

  // Wi-Fi: four equal cells from the RSSI; empty when the link is down.
  const uint8_t bars = s.link == Link::kUp ? derived::wifi_bars(s.rssi) : 0;
  x = draw_cells(g, static_cast<int16_t>(x - kGroupGap), static_cast<int16_t>(mid - kCellH / 2), bars);
  return draw_right(g, static_cast<int16_t>(x - kGap), baseline, "WIFI");
}

}  // namespace

void draw_footer(u8g2_t* g, const PanelState& s) {
  const DerivedModel& d = s.derived;
  u8g2_SetFont(g, fonts::kLabel);
  const int16_t baseline = centred_baseline(g, kFooter.y, kFooter.h);
  char upd[8], next[8], line[64];
  hhmm(upd, sizeof(upd), s.clock_valid, s.last_fetch_utc);
  hhmm(next, sizeof(next), s.clock_valid, s.next_fetch_utc);

  // Stale only once a forecast exists: before the first fetch the panel
  // is waiting, not stale.
  const bool stale = d.weather_valid && d.stale;
  if (stale) {
    fill(g, kFooter);
    u8g2_SetDrawColor(g, 0);
    snprintf(line, sizeof(line), "!! STALE \xC2\xB7 %s \xC2\xB7 UPD %s",
             s.link == Link::kUp ? "FETCH FAILING" : "LINK DOWN", upd);
  } else {
    snprintf(line, sizeof(line), "UPD %s \xC2\xB7 NEXT %s \xC2\xB7 %s", upd, next, s.rtc_ok ? "RTC OK" : "RTC UNSET");
  }
  u8g2_DrawUTF8(g, kLeft, baseline, line);
  draw_status_group(g, s, baseline);
  u8g2_SetDrawColor(g, 1);
}

}  // namespace ui
