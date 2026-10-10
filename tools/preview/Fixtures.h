// Fake PanelStates for the host preview. Public example data only: the real
// location never appears here.
#pragma once

#include <cstring>

#include "data/PanelState.h"

namespace fixtures {

// Matches the design/ mockup: KINGS LANGLEY, TUE 06 OCT 2026, 13:30:07 BST.
inline PanelState typical() {
  PanelState s;
  s.location_name = "KINGS LANGLEY";
  s.clock_valid = true;
  s.now_utc = 1791289807;
  s.rtc_ok = true;
  s.link = Link::kUp;
  s.rssi = -58;
  return s;
}

// Widest header text (AV-007): a 14-character name of the widest capital,
// the seconds clock, and a wide date (WED 30 SEP 23:59:59 BST).
inline PanelState worst() {
  PanelState s = typical();
  s.location_name = "WWWWWWWWWWWWWW";
  s.show_seconds = true;
  s.now_utc = 1790809199;
  return s;
}

// No trusted time yet: clock shows dashes.
inline PanelState unset() {
  PanelState s = typical();
  s.clock_valid = false;
  s.rtc_ok = false;
  s.link = Link::kConnecting;
  return s;
}

struct Named {
  const char* name;
  PanelState (*make)();
};

constexpr Named kAll[] = {{"typical", typical}, {"worst", worst}, {"unset", unset}};

}  // namespace fixtures
