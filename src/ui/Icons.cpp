#include "ui/Icons.h"

namespace ui {

namespace {

// Ray endpoints for a sun of radius r, as offsets for 8 directions at
// 45 degrees, scaled by 1/2 (cos 45 ~ 0.707 ~ 1/sqrt2, rounded per size).
struct Ray {
  int8_t x0, y0, x1, y1;
};

void sun(u8g2_t* g, int16_t cx, int16_t cy, bool small) {
  const int16_t r = small ? 5 : 8;
  u8g2_DrawCircle(g, cx, cy, r, U8G2_DRAW_ALL);
  static const Ray kBig[] = {{0, -12, 0, -17}, {0, 12, 0, 17}, {-12, 0, -17, 0}, {12, 0, 17, 0},
                             {8, -8, 12, -12}, {-8, -8, -12, -12}, {8, 8, 12, 12}, {-8, 8, -12, 12}};
  static const Ray kSmall[] = {{0, -8, 0, -11}, {0, 8, 0, 11}, {-8, 0, -11, 0}, {8, 0, 11, 0},
                               {6, -6, 8, -8}, {-6, -6, -8, -8}, {6, 6, 8, 8}, {-6, 6, -8, 8}};
  for (const Ray& ray : small ? kSmall : kBig) u8g2_DrawLine(g, cx + ray.x0, cy + ray.y0, cx + ray.x1, cy + ray.y1);
}

// Solid crescent: a disc with an offset disc erased from it.
void moon(u8g2_t* g, int16_t cx, int16_t cy, bool small) {
  const int16_t r = small ? 7 : 11;
  const int16_t off = small ? 4 : 6;
  u8g2_DrawDisc(g, cx, cy, r, U8G2_DRAW_ALL);
  u8g2_SetDrawColor(g, 0);
  u8g2_DrawDisc(g, cx + off, cy - off / 2, r - 1, U8G2_DRAW_ALL);
  u8g2_SetDrawColor(g, 1);
}

// Outline cloud, 30 x 17 px, top-left at (x, y). Drawn solid, then the
// interior erased, so anything behind it (sun, moon) is hidden.
void cloud(u8g2_t* g, int16_t x, int16_t y) {
  for (int pass = 0; pass < 2; ++pass) {
    const int16_t shrink = static_cast<int16_t>(pass);
    u8g2_SetDrawColor(g, pass == 0 ? 1 : 0);
    u8g2_DrawDisc(g, x + 8, y + 10, 6 - shrink, U8G2_DRAW_ALL);
    u8g2_DrawDisc(g, x + 16, y + 7, 7 - shrink, U8G2_DRAW_ALL);
    u8g2_DrawDisc(g, x + 23, y + 11, 5 - shrink, U8G2_DRAW_ALL);
    u8g2_DrawBox(g, x + 3 + shrink, y + 10, 24 - 2 * shrink, 6 - shrink);
  }
  u8g2_SetDrawColor(g, 1);
}

// Precipitation marks under a cloud whose top-left is (x, y).
void rain_streaks(u8g2_t* g, int16_t x, int16_t y, int count) {
  for (int i = 0; i < count; ++i) {
    const int16_t sx = static_cast<int16_t>(x + 9 + i * 7);
    u8g2_DrawLine(g, sx, y + 20, sx - 3, y + 27);
    u8g2_DrawLine(g, sx + 1, y + 20, sx - 2, y + 27);
  }
}

void drizzle_dots(u8g2_t* g, int16_t x, int16_t y) {
  for (int i = 0; i < 3; ++i) {
    u8g2_DrawBox(g, x + 9 + i * 7, y + 21, 2, 2);
    u8g2_DrawBox(g, x + 6 + i * 7, y + 26, 2, 2);
  }
}

void flake(u8g2_t* g, int16_t cx, int16_t cy) {
  u8g2_DrawLine(g, cx - 3, cy, cx + 3, cy);
  u8g2_DrawLine(g, cx - 2, cy - 2, cx + 2, cy + 2);
  u8g2_DrawLine(g, cx - 2, cy + 2, cx + 2, cy - 2);
  u8g2_DrawPixel(g, cx, cy - 3);
  u8g2_DrawPixel(g, cx, cy + 3);
}

void bolt(u8g2_t* g, int16_t x, int16_t y) {
  u8g2_DrawTriangle(g, x + 17, y + 17, x + 12, y + 26, x + 17, y + 26);
  u8g2_DrawTriangle(g, x + 15, y + 25, x + 20, y + 25, x + 13, y + 35);
}

}  // namespace

void draw_icon(u8g2_t* g, Icon icon, int16_t x, int16_t y) {
  // Precipitation icons put the cloud high; plain clouds sit lower.
  const int16_t px = static_cast<int16_t>(x + 5), py = static_cast<int16_t>(y + 3);
  switch (icon) {
    case Icon::kSun:
      sun(g, x + 20, y + 20, false);
      break;
    case Icon::kMoon:
      moon(g, x + 18, y + 20, false);
      break;
    case Icon::kSunCloud:
      sun(g, x + 14, y + 13, true);
      cloud(g, x + 8, y + 17);
      break;
    case Icon::kMoonCloud:
      moon(g, x + 14, y + 13, true);
      cloud(g, x + 8, y + 17);
      break;
    case Icon::kCloud:
      cloud(g, x + 5, y + 11);
      break;
    case Icon::kFog:
      for (int i = 0; i < 4; ++i) {
        const int16_t indent = (i % 2) ? 4 : 0;
        u8g2_DrawHLine(g, x + 6 + indent, y + 12 + i * 6, 28 - indent);
      }
      break;
    case Icon::kDrizzle:
      cloud(g, px, py);
      drizzle_dots(g, px, py);
      break;
    case Icon::kRain:
      cloud(g, px, py);
      rain_streaks(g, px, py, 3);
      break;
    case Icon::kRainIce:
      cloud(g, px, py);
      rain_streaks(g, px, py, 2);
      flake(g, px + 26, py + 24);
      break;
    case Icon::kSnow:
      cloud(g, px, py);
      flake(g, px + 8, py + 24);
      flake(g, px + 16, py + 30);
      flake(g, px + 24, py + 24);
      break;
    case Icon::kStorm:
      cloud(g, px, py);
      bolt(g, px, py);
      break;
    case Icon::kNone:
      u8g2_DrawFrame(g, x + 12, y + 12, 16, 16);
      break;
  }
}

}  // namespace ui
