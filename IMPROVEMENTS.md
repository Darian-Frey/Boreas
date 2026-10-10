# Improvements

Catalogue of code-quality improvements, refactors, and architectural
changes proposed during development. Per Maintenance Rule 8, improvements
are logged here when noticed, not silently applied. The author decides
whether to apply, defer, or decline.

This is the dual of BUGS.md: bugs are broken; improvements work but
could be better.

Status vocabulary: suggested | applied | declined | deferred.
Effort vocabulary: trivial | small | medium | large.

## Suggested

### IMP-001: Custom bitmap fonts via `bdfconv` for the badge and big numerals
**Status:** suggested
**Found:** 2026-10-10 (documentation scaffolding)
**Location:** `src/ui/fonts.h`, `assets/fonts/`
**Effort:** medium
**Description.** U8g2's built-in fonts are serviceable but none is a true 7-segment/dot-matrix pairing matching the mockup's VT323 / Share Tech Mono look. A converted BDF (e.g. a 7-segment face for the hero numerals, a 5×7 dot-matrix for labels) would land the aesthetic.
**Proposal.** Convert chosen open-licence BDF fonts with U8g2's `bdfconv` restricted to the glyph ranges used (`0-9 . - ° % : /` and caps), commit the generated `.c` arrays under `assets/fonts/`.
**Trade-offs.** Flash cost (small, with restricted ranges); a licence check per font; with D-016's host preview these fonts render exactly, so no preview gap. Do this only after the Phase 1 font pass shows the built-ins fall short.
**Notes.** Depends on D-005. Record the final font set in SPEC.md §Layout.

### IMP-004: Fail a Wi-Fi attempt as soon as the driver reports it, not after the 15 s timeout
**Status:** suggested
**Found:** 2026-10-10 (first on-board run of `src/net/WiFiMgr`)
**Location:** `src/net/WiFiMgr.cpp` (`State::kConnecting`)
**Effort:** small
**Description.** After a reset, the router still held the board's old session and refused the first association. The driver logged `Association refused too many times` at 1.6 s, but `WiFiMgr` waited for its own 15 s timeout before backing off, so Wi-Fi came up at 21.8 s instead of about 7 s.
**Proposal.** In `kConnecting`, treat `WL_CONNECT_FAILED` / `WL_NO_SSID_AVAIL` from `WiFi.status()` as an immediate failure. Check on the board first which status the driver actually reports after a refused association.
**Trade-offs.** Saves about 15 s per reboot only. Small risk of reacting to a transient status during association, which would just retry 5 s sooner. The clock is unaffected, since the RTC shows the time meanwhile.

## Applied

### IMP-002: Pin exact library versions after Phase 0
**Status:** applied
**Found:** 2026-10-10
**Location:** `platformio.ini`
**Effort:** trivial
**Description.** BUILD.md's reference `lib_deps` uses caret ranges so the first build succeeds; AV-001 wants exact pins.
**Proposal.** Once bring-up passes, replace `^` with the exact versions that worked and note them in BUILD.md.
**Trade-offs.** Exact pins must be bumped by hand to get fixes; that is the point.
**Applied.** 2026-10-10: pioarduino release `55.03.312` instead of `stable`, ArduinoJson `7.4.3`, SensorLib `0.3.1`, U8g2 commit `b3a40de`. Bring-up re-run on the board with the pinned set; passes.


## Declined

### IMP-003: Host-side renderer shares the real `grid.h`
**Status:** declined
**Found:** 2026-10-10
**Location:** `tools/preview.py`, `src/ui/grid.h`
**Effort:** small
**Description.** If `preview.py` keeps its own copy of the grid constants, it will drift from the firmware (AV-007).
**Proposal.** Have `preview.py` parse `grid.h` (simple `constexpr int` lines) at run time rather than duplicating values; or generate both from one `grid.toml`.
**Trade-offs.** A small parser to maintain; worth it to keep one source of truth (Maintenance Rule 5).
**Declined.** 2026-10-10: superseded by D-016. The host preview compiles the real `src/ui/grid.h`, so there is no second copy to keep in sync.


## Deferred

_None._
