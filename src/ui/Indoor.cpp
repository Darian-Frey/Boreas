#include "ui/Indoor.h"

#include <cstdio>

#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/Icons.h"
#include "ui/grid.h"

namespace ui {

namespace {

using namespace grid;

// Column content below the tab: x 151..270, y 38..198.
constexpr int16_t kLeft = kIntCol.x + kPad;                               // 154
constexpr int16_t kRight = static_cast<int16_t>(kIntCol.right() - kPad);  // 267
constexpr int16_t kWidth = kRight - kLeft + 1;                            // 114

constexpr int16_t kTempBaseline = 64;
constexpr int16_t kRule1Y = 69;
constexpr int16_t kHumBaseline = 96;
constexpr int16_t kRule2Y = 101;
constexpr int16_t kMeterLabelBaseline = 112;
constexpr int16_t kMeterY = 116;
constexpr int16_t kMeterH = 10;
constexpr int16_t kMeterCells = 10;
constexpr int16_t kCellW = 10;
constexpr int16_t kCellGap = 1;
constexpr int16_t kBandTickY = kMeterY + kMeterH + 1;  // brackets under the 30-60 % cells
constexpr int16_t kRule3Y = 133;
constexpr int16_t kPairLabelBaseline = 145;
constexpr int16_t kPairValueBaseline = 158;
constexpr int16_t kPairSecondX = kLeft + 60;
constexpr Rect kComfortBox{kLeft, 170, kWidth, 22};
constexpr int16_t kBandLo = 3, kBandHi = 5;  // cells 30-40, 40-50, 50-60 %

void dotted_rule(u8g2_t* g, int16_t y) {
  for (int16_t x = kLeft; x <= kRight; x += 3) u8g2_DrawPixel(g, x, y);
}

// Label left; big value with a small unit right-aligned on one baseline.
void value_row(u8g2_t* g, int16_t baseline, const char* label, const char* value, const char* unit) {
  u8g2_SetFont(g, fonts::kLabel);
  u8g2_DrawStr(g, kLeft, baseline, label);
  const int16_t unit_x = draw_right(g, kRight, baseline, unit);
  u8g2_SetFont(g, fonts::kIndoor);
  draw_right(g, static_cast<int16_t>(unit_x - 3), baseline, value);
}

}  // namespace

void draw_indoor(u8g2_t* g, const PanelState& s) {
  const IndoorModel& in = s.indoor;
  const DerivedModel& d = s.derived;
  char buf[24];

  // Temperature and humidity
  if (in.valid) snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(in.temp_c));
  value_row(g, kTempBaseline, "TEMP", in.valid ? buf : "--.-", "\xC2\xB0" "C");
  dotted_rule(g, kRule1Y);
  if (in.valid) snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(in.rh));
  value_row(g, kHumBaseline, "HUMID", in.valid ? buf : "--", "%");
  dotted_rule(g, kRule2Y);

  // RH level meter: filled cells up to the reading; ticks bracket the
  // 30-60 % comfort band underneath.
  u8g2_SetFont(g, fonts::kLabel);
  u8g2_DrawStr(g, kLeft, kMeterLabelBaseline, "RH LEVEL");
  draw_right(g, kRight, kMeterLabelBaseline, "30\xC2\xB7" "60");
  for (int16_t i = 0; i < kMeterCells; ++i) {
    const int16_t x = static_cast<int16_t>(kLeft + 2 + i * (kCellW + kCellGap));
    if (in.valid && i < d.rh_segments)
      u8g2_DrawBox(g, x, kMeterY, kCellW, kMeterH);
    else
      u8g2_DrawFrame(g, x, kMeterY, kCellW, kMeterH);
  }
  const int16_t band_x0 = static_cast<int16_t>(kLeft + 2 + kBandLo * (kCellW + kCellGap));
  const int16_t band_x1 = static_cast<int16_t>(kLeft + 2 + (kBandHi + 1) * (kCellW + kCellGap) - kCellGap - 1);
  u8g2_DrawHLine(g, band_x0, kBandTickY + 2, band_x1 - band_x0 + 1);
  u8g2_DrawVLine(g, band_x0, kBandTickY, 3);
  u8g2_DrawVLine(g, band_x1, kBandTickY, 3);
  dotted_rule(g, kRule3Y);

  // Delta in/out and dew point, side by side.
  draw_delta(g, kLeft, static_cast<int16_t>(kPairLabelBaseline - 7));
  u8g2_DrawStr(g, kLeft + 10, kPairLabelBaseline, "IN/OUT");
  u8g2_DrawStr(g, kPairSecondX, kPairLabelBaseline, "DEW");
  if (d.delta_valid)
    snprintf(buf, sizeof(buf), "%+.1f\xC2\xB0", static_cast<double>(d.delta_c));
  else
    snprintf(buf, sizeof(buf), "--");
  u8g2_DrawUTF8(g, kLeft, kPairValueBaseline, buf);
  if (in.valid)
    snprintf(buf, sizeof(buf), "%.1f\xC2\xB0", static_cast<double>(d.indoor_dew_c));
  else
    snprintf(buf, sizeof(buf), "--");
  u8g2_DrawUTF8(g, kPairSecondX, kPairValueBaseline, buf);

  // Comfort box: outlined when NOMINAL, inverted when out of band.
  const bool alert = d.comfort != Comfort::kNominal && d.comfort != Comfort::kUnknown;
  snprintf(buf, sizeof(buf), "COMFORT \xC2\xB7 %s", derived::comfort_text(d.comfort));
  if (alert) {
    fill(g, kComfortBox);
    u8g2_SetDrawColor(g, 0);
  } else {
    u8g2_DrawFrame(g, kComfortBox.x, kComfortBox.y, kComfortBox.w, kComfortBox.h);
  }
  const int16_t tw = text_width(g, buf);
  u8g2_DrawUTF8(g, static_cast<int16_t>(kComfortBox.x + (kComfortBox.w - tw) / 2),
                centred_baseline(g, kComfortBox.y, kComfortBox.h), buf);
  u8g2_SetDrawColor(g, 1);
}

}  // namespace ui
