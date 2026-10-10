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
#include "ui/Frame.h"
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

const Scene kScenes[] = {
    {"test", scene_test},
    {"frame", scene_frame},
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
