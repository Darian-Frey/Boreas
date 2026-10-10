// The panel layout grid (SPEC.md §Layout). Every pixel position in ui/
// comes from here. Change it only together with SPEC.md §Layout and the
// reference image in design/ (CLAUDE.md §Out of scope).
//
// Panel is 400x300 landscape. A double border (outer line at 3, inner
// line at 5, 1 px each) encloses a table of 1 px rules:
//
//   x:  5 | 6 ...... 149 | 150 | 151 ... 270 | 271 | 272 ..... 393 | 394
//   y:  5..24 header box (inverted)
//       25        white gap
//       26..198   body: EXT | INT | READOUTS columns, rules at x 150, 271
//       199       rule
//       200..278  bottom strip: trace (left) + rain meter (right)
//       279       rule
//       280..293  footer
//       294       inner border
#pragma once

#include <cstdint>

namespace ui {
namespace grid {

struct Rect {
  int16_t x, y, w, h;
  constexpr int16_t right() const { return static_cast<int16_t>(x + w - 1); }   // last column
  constexpr int16_t bottom() const { return static_cast<int16_t>(y + h - 1); }  // last row
};

// Border lines
constexpr Rect kOuterBorder{3, 3, 394, 294};
constexpr Rect kInnerBorder{5, 5, 390, 290};

// Header bar, drawn inverted over the inner border's top edge.
constexpr Rect kHeader{5, 5, 390, 20};

// Body columns: content areas between the rules.
constexpr int16_t kBodyTop = 26;
constexpr int16_t kBodyHeight = 173;  // rows 26..198
constexpr Rect kExtCol{6, kBodyTop, 144, kBodyHeight};
constexpr Rect kIntCol{151, kBodyTop, 120, kBodyHeight};
constexpr Rect kReadCol{272, kBodyTop, 122, kBodyHeight};
constexpr int16_t kColRuleX1 = 150;
constexpr int16_t kColRuleX2 = 271;

// Inverted section tab at the top of each column, inset 1 px from the
// rules so neighbouring tabs never merge into one bar.
constexpr int16_t kTabHeight = 12;
constexpr int16_t kTabInset = 1;

// Bottom strip and footer
constexpr int16_t kBodyRuleY = 199;
constexpr Rect kStrip{6, 200, 388, 79};
constexpr Rect kTrace{6, 200, 250, 79};    // left 250 px
constexpr Rect kRain{256, 200, 138, 79};   // the rest
constexpr int16_t kStripRuleY = 279;
constexpr Rect kFooter{6, 280, 388, 14};

// Padding inside regions
constexpr int16_t kPad = 3;

}  // namespace grid
}  // namespace ui
