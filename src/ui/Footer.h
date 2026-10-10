// ui::draw_footer: the status line (F-007): last and next fetch, RTC
// state, Wi-Fi bars, battery gauge and voltage. Becomes an inverted
// "!! STALE" bar when the forecast is older than 2x the fetch interval.
#pragma once

#include <clib/u8g2.h>

#include "data/PanelState.h"

namespace ui {

void draw_footer(u8g2_t* g, const PanelState& s);

}  // namespace ui
