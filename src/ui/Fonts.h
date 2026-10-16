// Font roles (SPEC.md §Layout). Every font in ui/ is chosen here.
//
// The set is U8g2 built-ins only (IMP-001): bold logisoso numerals for the
// big values, the VCR_OSD on-screen-display face for the header clock, and
// the spleen terminal faces for labels and readouts. Glyph coverage matters:
// _tr fonts are ASCII only, so text with a degree sign or middle dot uses a
// font that has them (spleen _mf, 6x12 _tf) and u8g2_DrawUTF8.
#pragma once

#include <clib/u8g2.h>

namespace ui {
namespace fonts {

inline const uint8_t* const kHeader = u8g2_font_7x14B_tr;        // badge, location, date
inline const uint8_t* const kClock = u8g2_font_VCR_OSD_mn;       // header clock (digits and colon only)
inline const uint8_t* const kLabel = u8g2_font_spleen6x12_mf;    // tabs, labels, values, footer (has ° ·)
inline const uint8_t* const kSource = u8g2_font_spleen5x8_mf;    // small tab source labels
inline const uint8_t* const kSmall = u8g2_font_spleen6x12_mf;    // spare small text
inline const uint8_t* const kReadout = u8g2_font_spleen8x16_mf;  // READOUTS labels and values
inline const uint8_t* const kUnit = u8g2_font_spleen8x16_mf;     // the C after the outdoor degree sign
inline const uint8_t* const kHero = u8g2_font_logisoso50_tn;     // outdoor temperature, >= 48 px (F-002), digits only
inline const uint8_t* const kIndoor = u8g2_font_logisoso24_tn;   // indoor values (digits only)

}  // namespace fonts
}  // namespace ui
