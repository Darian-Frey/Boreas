#include "ui/TestCard.h"

#include "ui/Icons.h"

namespace ui {

void draw_test_card(u8g2_t* g) {
  u8g2_ClearBuffer(g);
  u8g2_SetFontMode(g, 1);
  u8g2_SetDrawColor(g, 1);

  // Double outer frame and inverted header bar (SPEC.md §Layout).
  u8g2_DrawFrame(g, 3, 3, 394, 294);
  u8g2_DrawFrame(g, 5, 5, 390, 290);
  u8g2_DrawBox(g, 5, 5, 390, 20);
  u8g2_SetDrawColor(g, 0);
  u8g2_SetFont(g, u8g2_font_7x14B_tr);
  u8g2_DrawStr(g, 10, 20, "BOREAS MS-42 // TEST CARD");
  u8g2_SetDrawColor(g, 1);

  // Label font with Latin-1 glyphs: the _tf variant has the degree sign and
  // middle dot that _tr lacks.
  u8g2_SetFont(g, u8g2_font_6x12_tf);
  u8g2_DrawUTF8(g, 12, 42, "EXT \xC2\xB7 OUTDOOR   21.4\xC2\xB0" "C   -10.5   100 %");
  u8g2_DrawUTF8(g, 12, 56, "FEELS 12\xC2\xB0 \xC2\xB7 HI 17 \xC2\xB7 LO 09");
  draw_delta(g, 12, 63);
  u8g2_DrawStr(g, 22, 70, "IN/OUT +7.4");
  draw_up_arrow(g, 110, 63);
  draw_down_arrow(g, 124, 63);
  u8g2_DrawStr(g, 138, 70, "PRESS 1009");

  u8g2_SetFont(g, u8g2_font_profont11_tr);
  u8g2_DrawStr(g, 12, 86, "profont11: WIND SW 18  GUST 31");

  // Hero numerals: digits only, so the degree sign is a small circle.
  u8g2_SetFont(g, u8g2_font_logisoso42_tn);
  u8g2_DrawStr(g, 12, 150, "-10.5");
  u8g2_DrawCircle(g, 140, 112, 4, U8G2_DRAW_ALL);
  u8g2_SetFont(g, u8g2_font_logisoso24_tn);
  u8g2_DrawStr(g, 12, 190, "21.4");

  // Checkerboard and 1 px line groups (1, 2, 3 px gaps).
  for (int cy = 0; cy < 8; ++cy)
    for (int cx = 0; cx < 8; ++cx)
      if ((cx + cy) & 1) u8g2_DrawBox(g, 250 + cx * 16, 40 + cy * 16, 16, 16);
  for (int gap = 1; gap <= 3; ++gap) {
    const int y0 = 180 + (gap - 1) * 24;
    for (int i = 0; i < 4; ++i) u8g2_DrawHLine(g, 250, y0 + i * (gap + 1), 100);
  }

  // Dotted grid as in the trace (every 4 px).
  for (int y = 210; y <= 270; y += 12)
    for (int x = 12; x <= 230; x += 4) u8g2_DrawPixel(g, x, y);

  // Corner markers: all four must be visible.
  u8g2_DrawBox(g, 0, 0, 3, 3);
  u8g2_DrawBox(g, 397, 0, 3, 3);
  u8g2_DrawBox(g, 0, 297, 3, 3);
  u8g2_DrawBox(g, 397, 297, 3, 3);
}

}  // namespace ui
