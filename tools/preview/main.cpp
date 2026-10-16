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
#include "ui/Footer.h"
#include "ui/Fonts.h"
#include "ui/Frame.h"
#include "ui/Icons.h"
#include "ui/Indoor.h"
#include "ui/Outdoor.h"
#include "ui/RainMeter.h"
#include "ui/Readouts.h"
#include "ui/TestCard.h"
#include "ui/Trace.h"

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
  ui::draw_trace(g, s);
  ui::draw_rain_meter(g, s);
  ui::draw_footer(g, s);
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

// IMP-001 font sheets: candidate faces with the panel's own text, so a
// font can be chosen by eye. Glyph heights are measured, and missing
// degree signs or middle dots are flagged.
struct FontCandidate {
  const char* name;
  const uint8_t* font;
};

void draw_font_row(u8g2_t* g, int16_t y, const FontCandidate& c, const char* const* samples, int n) {
  u8g2_SetFont(g, ui::fonts::kSource);
  u8g2_DrawStr(g, 4, y - 2, c.name);
  u8g2_SetFont(g, c.font);
  int16_t x = 4;
  const int16_t base = static_cast<int16_t>(y + u8g2_GetAscent(g) + 2);
  for (int i = 0; i < n; ++i) {
    const int16_t w = static_cast<int16_t>(u8g2_GetUTF8Width(g, samples[i]));
    u8g2_DrawUTF8(g, x, base, samples[i]);
    x = static_cast<int16_t>(x + w + 14);
  }
  char note[48];
  const bool deg = u8g2_IsGlyph(g, 0xB0), dot = u8g2_IsGlyph(g, 0xB7);
  snprintf(note, sizeof(note), "%dpx%s%s", u8g2_GetAscent(g), deg ? "" : " no-deg", dot ? "" : " no-dot");
  u8g2_SetFont(g, ui::fonts::kSource);
  ui::draw_right(g, 396, static_cast<int16_t>(y - 2), note);
}

void scene_fonts_hero(u8g2_t* g, const PanelState&) {
  static const FontCandidate kHero[] = {
      {"logisoso50_tn (current)", u8g2_font_logisoso50_tn},
      {"7Segments_26x42_mn", u8g2_font_7Segments_26x42_mn},
      {"7_Seg_41x21_mn", u8g2_font_7_Seg_41x21_mn},
      {"spleen32x64_mn", u8g2_font_spleen32x64_mn},
  };
  static const char* const kSamples[] = {"14", "-25", "21.4"};
  u8g2_ClearBuffer(g);
  int16_t y = 8;
  for (const FontCandidate& c : kHero) {
    draw_font_row(g, y, c, kSamples, 3);
    u8g2_SetFont(g, c.font);
    y = static_cast<int16_t>(y + u8g2_GetAscent(g) + 12);
  }
}

void scene_fonts_label(u8g2_t* g, const PanelState&) {
  static const FontCandidate kLabel[] = {
      {"6x12_tf (current labels)", u8g2_font_6x12_tf},
      {"7x14B_tr (current readouts)", u8g2_font_7x14B_tr},
      {"VCR_OSD_tf", u8g2_font_VCR_OSD_tf},
      {"spleen6x12_mf", u8g2_font_spleen6x12_mf},
      {"spleen8x16_mf", u8g2_font_spleen8x16_mf},
      {"9x6LED_tf", u8g2_font_9x6LED_tf},
      {"12x6LED_tf", u8g2_font_12x6LED_tf},
      {"Terminal_tr", u8g2_font_Terminal_tr},
  };
  static const char* const kSamples[] = {"EXT \xC2\xB7 OUTDOOR", "WIND SW 18", "12\xC2\xB0"};
  u8g2_ClearBuffer(g);
  int16_t y = 8;
  for (const FontCandidate& c : kLabel) {
    draw_font_row(g, y, c, kSamples, 3);
    u8g2_SetFont(g, c.font);
    y = static_cast<int16_t>(y + u8g2_GetAscent(g) + 16);
  }
}

// IMP-001: would VCR_OSD fit each text region? Worst-case strings measured
// against the space the region offers (and its height).
void scene_vcr_fit(u8g2_t* g, const PanelState&) {
  static const struct {
    const char* region;
    const char* text;
    int16_t width;   // px available
    int16_t height;  // px available for the glyph height
  } kRows[] = {
      {"header title", "BOREAS MS-42 // WWWWWWWWWWWWWW", 250, 20},
      {"header date", "WED 30 SEP", 110, 20},
      {"tab EXT", "EXT \xC2\xB7 OUTDOOR", 100, 12},
      {"feels row", "FEELS -16\xC2\xB0", 91, 13},
      {"condition", "MODERATE RAIN", 138, 22},
      {"indoor label", "HUMID", 50, 13},
      {"readout row", "UV 11 EXTREME", 116, 22},
      {"readout row", "WIND NW 118", 116, 22},
      {"comfort box", "COMFORT \xC2\xB7 NOMINAL", 110, 22},
      {"advisory box", "UMBRELLA ADVISED", 128, 23},
      {"trace title", "TEMP \xC2\xB7 NEXT 12 H", 150, 12},
      {"footer left", "UPD 13:00 \xC2\xB7 NEXT 13:30 \xC2\xB7 RTC OK", 220, 14},
  };
  u8g2_ClearBuffer(g);
  int16_t y = 10;
  char line[64];
  for (const auto& r : kRows) {
    u8g2_SetFont(g, u8g2_font_VCR_OSD_tf);
    const int16_t w = static_cast<int16_t>(u8g2_GetUTF8Width(g, r.text));
    const int16_t h = static_cast<int16_t>(u8g2_GetAscent(g) - u8g2_GetDescent(g));
    const bool fits = w <= r.width && h <= r.height;
    u8g2_SetFont(g, ui::fonts::kSource);
    snprintf(line, sizeof(line), "%-13s %3d/%3d px wide %2d/%2d high  %s", r.region, w, r.width, h, r.height,
             fits ? "FITS" : "NO");
    u8g2_DrawStr(g, 4, y, line);
    y = static_cast<int16_t>(y + 10);
  }
}

const Scene kScenes[] = {
    {"test", scene_test},   {"frame", scene_frame},           {"main", scene_main},
    {"icons", scene_icons}, {"feels-oneline", scene_feels_oneline},
    {"fonts-hero", scene_fonts_hero}, {"fonts-label", scene_fonts_label}, {"vcr-fit", scene_vcr_fit},
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
