// Host-native geometry checks for src/ui/grid.h (AV-007, pio test -e native).
// Text fit is checked by rendering fixtures in the host preview (D-016).

#include <unity.h>

#include <initializer_list>

#include "ui/grid.h"

using ui::grid::Rect;
namespace g = ui::grid;

void setUp() {}
void tearDown() {}

static bool inside(const Rect& inner, const Rect& outer) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.right() <= outer.right() && inner.bottom() <= outer.bottom();
}

static bool overlap(const Rect& a, const Rect& b) {
  return a.x <= b.right() && b.x <= a.right() && a.y <= b.bottom() && b.y <= a.bottom();
}

// Content area strictly inside the inner border line.
static const Rect kContent{static_cast<int16_t>(g::kInnerBorder.x + 1), static_cast<int16_t>(g::kInnerBorder.y + 1),
                           static_cast<int16_t>(g::kInnerBorder.w - 2), static_cast<int16_t>(g::kInnerBorder.h - 2)};

void test_borders_on_panel() {
  const Rect panel{0, 0, 400, 300};
  TEST_ASSERT_TRUE(inside(g::kOuterBorder, panel));
  TEST_ASSERT_TRUE(inside(g::kInnerBorder, g::kOuterBorder));
  TEST_ASSERT_EQUAL_INT16(2, g::kInnerBorder.x - g::kOuterBorder.x);  // 1 px gap between lines
}

void test_regions_inside_inner_border() {
  for (const Rect& r : {g::kExtCol, g::kIntCol, g::kReadCol, g::kStrip, g::kTrace, g::kRain, g::kFooter})
    TEST_ASSERT_TRUE(inside(r, kContent));
  TEST_ASSERT_TRUE(inside(g::kHeader, g::kInnerBorder));  // header is drawn over the border's top edge
}

void test_regions_do_not_overlap() {
  const Rect all[] = {g::kHeader, g::kExtCol, g::kIntCol, g::kReadCol, g::kTrace, g::kRain, g::kFooter};
  for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
    for (size_t j = i + 1; j < sizeof(all) / sizeof(all[0]); ++j) TEST_ASSERT_FALSE(overlap(all[i], all[j]));
}

void test_rules_sit_between_regions() {
  TEST_ASSERT_EQUAL_INT16(g::kExtCol.right() + 1, g::kColRuleX1);
  TEST_ASSERT_EQUAL_INT16(g::kColRuleX1 + 1, g::kIntCol.x);
  TEST_ASSERT_EQUAL_INT16(g::kIntCol.right() + 1, g::kColRuleX2);
  TEST_ASSERT_EQUAL_INT16(g::kColRuleX2 + 1, g::kReadCol.x);
  TEST_ASSERT_EQUAL_INT16(g::kReadCol.right(), kContent.right());
  TEST_ASSERT_EQUAL_INT16(g::kHeader.bottom() + 2, g::kBodyTop);  // one white row under the header
  TEST_ASSERT_EQUAL_INT16(g::kExtCol.bottom() + 1, g::kBodyRuleY);
  TEST_ASSERT_EQUAL_INT16(g::kBodyRuleY + 1, g::kStrip.y);
  TEST_ASSERT_EQUAL_INT16(g::kStrip.bottom() + 1, g::kStripRuleY);
  TEST_ASSERT_EQUAL_INT16(g::kStripRuleY + 1, g::kFooter.y);
  TEST_ASSERT_EQUAL_INT16(g::kFooter.bottom(), kContent.bottom());
}

void test_strip_split() {
  TEST_ASSERT_EQUAL_INT16(250, g::kTrace.w);  // SPEC.md: trace left 250 px
  TEST_ASSERT_EQUAL_INT16(g::kTrace.right() + 1, g::kRain.x);
  TEST_ASSERT_EQUAL_INT16(g::kStrip.right(), g::kRain.right());
}

void test_tabs_fit_columns() {
  for (const Rect& c : {g::kExtCol, g::kIntCol, g::kReadCol}) TEST_ASSERT_GREATER_THAN(g::kTabHeight, c.h);
  TEST_ASSERT_GREATER_THAN(0, g::kTabInset);  // a white column either side of each rule
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_borders_on_panel);
  RUN_TEST(test_regions_inside_inner_border);
  RUN_TEST(test_regions_do_not_overlap);
  RUN_TEST(test_rules_sit_between_regions);
  RUN_TEST(test_strip_split);
  RUN_TEST(test_tabs_fit_columns);
  return UNITY_END();
}
