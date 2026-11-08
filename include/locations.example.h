// Copy to include/locations.h (git-ignored) and set your own location.
// The real location must never be committed: locations.h is in .gitignore.
//
// The forecast is fetched for kHome. `name` is shown in the header bar
// (`BOREAS MS-42 // <name>`): capitals, at most 14 characters.
// Coordinates are decimal degrees, WGS84; west is negative.
#pragma once

namespace locations {

struct Location {
  const char* name;
  float latitude;
  float longitude;
};

// Public example location (the one used throughout the docs).
constexpr Location kLocations[] = {
    {"KINGS LANGLEY", 51.713f, -0.455f},
};

constexpr const Location& kHome = kLocations[0];

// Met Office severe weather warnings (F-019, D-021). kWarningRegion is the
// regional feed code (ee, se, sw, wm, em, nw, ne, yh, wl, ni, st, ta, dg, gr,
// he, os: SPEC.md §Met Office warnings); kWarningArea must match one entry
// of the feed's area list exactly (any case), usually the county or unitary
// authority.
constexpr const char* kWarningRegion = "ee";
constexpr const char* kWarningArea = "Hertfordshire";

}  // namespace locations
