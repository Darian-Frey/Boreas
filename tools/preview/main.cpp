// Boreas host preview (D-016): renders ui/ scenes with the real U8g2 code
// and fonts into a MemCanvas and writes a PNG, without the board.
//
//   pio run -e preview
//   .pio/build/preview/program [--scene NAME] [--fixture NAME] [--scale N] [--rlcd] [-o OUT.png]
//
// Prints the canvas hash; the board prints the same hash for the same
// scene, which proves the two builds draw identical pixels.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Fixtures.h"
#include "Png.h"
#include "config.h"
#include "display/MemCanvas.h"
#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/Frame.h"
#include "ui/Icons.h"
#include "ui/Indoor.h"
#include "ui/Outdoor.h"
#include "ui/Readouts.h"
#include "ui/TestCard.h"

namespace {

struct Scene {
  const char* name;
  void (*draw)(u8g2_t*, const PanelState&);
};

void scene_test(u8g2_t* g, const PanelState&) { ui::draw_test_card(g); }

void scene_frame(u8g2_t* g, const PanelState& s) {
  u8g2_ClearBuffer(g);
  ui::draw_frame(g, s);
}

void scene_main(u8g2_t* g, const PanelState& s) {
  u8g2_ClearBuffer(g);
  ui::draw_frame(g, s);
  ui::draw_outdoor(g, s);
  ui::draw_indoor(g, s);
  ui::draw_readouts(g, s);
}

// Every condition icon, labelled, for review.
void scene_icons(u8g2_t* g, const PanelState&) {
  static const struct {
    Icon icon;
    const char* name;
  } kAll[] = {{Icon::kSun, "SUN"},         {Icon::kMoon, "MOON"},       {Icon::kSunCloud, "SUN-CLOUD"},
              {Icon::kMoonCloud, "MOON-CLOUD"}, {Icon::kCloud, "CLOUD"},     {Icon::kFog, "FOG"},
              {Icon::kDrizzle, "DRIZZLE"},  {Icon::kRain, "RAIN"},       {Icon::kRainIce, "RAIN-ICE"},
              {Icon::kSnow, "SNOW"},       {Icon::kStorm, "STORM"},     {Icon::kNone, "NONE"}};
  u8g2_ClearBuffer(g);
  u8g2_SetFont(g, ui::fonts::kSource);
  for (int i = 0; i < 12; ++i) {
    const int16_t x = static_cast<int16_t>(20 + (i % 6) * 62), y = static_cast<int16_t>(40 + (i / 6) * 110);
    u8g2_DrawFrame(g, x - 1, y - 1, ui::kIconSize + 2, ui::kIconSize + 2);  // shows the 40x40 box
    ui::draw_icon(g, kAll[i].icon, x, y);
    u8g2_DrawStr(g, x, y + 54, kAll[i].name);
  }
}

// F-002's "feels, high, low on one line" at two font sizes, against the
// 138 px the EXT column offers. Bars mark the column's usable width.
void scene_feels_oneline(u8g2_t* g, const PanelState&) {
  static const char* const kLines[] = {"FEELS 12\xC2\xB0 \xC2\xB7 HI 17 \xC2\xB7 LO 09",
                                       "FEELS -16\xC2\xB0 \xC2\xB7 HI -10 \xC2\xB7 LO -28"};
  u8g2_ClearBuffer(g);
  int16_t y = 30;
  for (const uint8_t* font : {ui::fonts::kLabel, u8g2_font_5x7_tf}) {
    for (const char* line : kLines) {
      u8g2_DrawVLine(g, 9, y - 12, 16);
      u8g2_DrawVLine(g, 9 + 138, y - 12, 16);
      u8g2_SetFont(g, font);
      u8g2_DrawUTF8(g, 9, y, line);
      char w[16];
      snprintf(w, sizeof(w), "%d px", ui::text_width(g, line));
      u8g2_SetFont(g, ui::fonts::kSource);
      u8g2_DrawStr(g, 170, y, w);
      y = static_cast<int16_t>(y + 30);
    }
  }
}

const Scene kScenes[] = {
    {"test", scene_test},   {"frame", scene_frame},           {"main", scene_main},
    {"icons", scene_icons}, {"feels-oneline", scene_feels_oneline},
};

int usage() {
  std::fprintf(stderr, "usage: program [--scene NAME] [--fixture NAME] [--scale N] [--rlcd] [-o OUT.png]\nscenes:");
  for (const Scene& s : kScenes) std::fprintf(stderr, " %s", s.name);
  std::fprintf(stderr, "\nfixtures:");
  for (const fixtures::Named& f : fixtures::kAll) std::fprintf(stderr, " %s", f.name);
  std::fprintf(stderr, "\n");
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  const char* scene_name = "test";
  const char* fixture_name = "typical";
  const char* out = "preview.png";
  int scale = 1;
  bool rlcd = false;  // approximate the reflective panel's grey paper and dark ink
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--scene") && i + 1 < argc) scene_name = argv[++i];
    else if (!std::strcmp(argv[i], "--fixture") && i + 1 < argc) fixture_name = argv[++i];
    else if (!std::strcmp(argv[i], "--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
    else if (!std::strcmp(argv[i], "--rlcd")) rlcd = true;
    else return usage();
  }
  if (scale < 1 || scale > 8) return usage();

  const Scene* scene = nullptr;
  for (const Scene& s : kScenes)
    if (!std::strcmp(s.name, scene_name)) scene = &s;
  if (!scene) return usage();
  const fixtures::Named* fixture = nullptr;
  for (const fixtures::Named& f : fixtures::kAll)
    if (!std::strcmp(f.name, fixture_name)) fixture = &f;
  if (!fixture) return usage();

  // Same TZ rule as the board, so local times in ui/ match (config.h).
  setenv("TZ", config::kTimezone, 1);
  tzset();

  static MemCanvas canvas;
  const PanelState state = fixture->make();
  scene->draw(canvas.u8g2(), state);

  const uint8_t ink = rlcd ? 0x2A : 0x00;
  const uint8_t paper = rlcd ? 0xC4 : 0xFF;
  const int w = MemCanvas::kWidth * scale, h = MemCanvas::kHeight * scale;
  std::vector<uint8_t> grey(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) grey[static_cast<size_t>(y) * w + x] = canvas.pixel(x / scale, y / scale) ? ink : paper;

  if (!png::write_grey(out, w, h, grey)) {
    std::fprintf(stderr, "cannot write %s\n", out);
    return 1;
  }
  std::printf("scene %s / %s: hash %08x -> %s (%dx%d)\n", scene->name, fixture->name, canvas.hash(), out, w, h);
  return 0;
}
