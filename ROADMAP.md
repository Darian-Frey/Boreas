# Roadmap

Phases are append-only. Mark Complete with an ISO date.

## Phase 0 — Board bring-up
**Goal:** Prove the brand-new board, the host toolchain and every peripheral the project needs, before writing any Boreas code.
**Status:** Not started
**Features delivered:** (none — this phase establishes the build path in BUILD.md and seeds ATTACK_VECTORS.md)
**Deliverables:**
- [ ] Unbox, fit 18650, first power-on; note the factory firmware behaviour (BUILD.md §First power-on)
- [ ] Linux host sees the board on USB-C (`/dev/ttyACM0`), user in `dialout`, `brltty` conflict ruled out
- [ ] PlatformIO + pioarduino environment compiles and uploads a blink (BUILD.md §Toolchain)
- [ ] Waveshare `05_I2C_SHTC3` and `04_I2C_PCF85063` ported into `test/bringup/` and verified on serial
- [ ] U8g2 ST7305 test: full-buffer draw at 400×300 landscape, HPM→LPM switch observed
- [ ] `03_ADC_Test` equivalent: battery voltage reads plausibly with and without USB
- [ ] Button test: BOOT (GPIO0), KEY (GPIO18) and the add-on location button (GPIO17, header P1-13, if fitted) each log press and release on serial, with no spurious or bouncing events (SPEC.md §Pin map)
- [ ] I2C scan of GPIO13/14 lists 0x51 (RTC) and 0x70 (SHTC3); note any other addresses, e.g. a QMI8658C, in SPEC.md
- [ ] Measured current draw: idle in LPM, during fetch, during redraw — recorded in SPEC.md §Power
**Acceptance:** `pio run -t upload` of `test/bringup` shows all peripherals passing on serial and a test pattern held on the panel in LPM.

## Phase 1 — Live panel (mains)
**Goal:** The full Main page on the panel, fed by real data, running indefinitely on USB power.
**Status:** Not started
**Features delivered:** F-001, F-002, F-003, F-004, F-005, F-006, F-007, F-008, F-009, F-010, F-011
**Deliverables:**
- [ ] `src/net/` — Wi-Fi station with reconnect; NTP + TZ; RTC sync
- [ ] `src/data/` — Open-Meteo client with JSON filter; `WeatherModel` struct; stale/retry logic
- [ ] `src/sensors/` — SHTC3 reader, derived values (dew point, Δ, comfort band); battery ADC
- [ ] `src/ui/` — layout engine on U8g2: frame, columns, trace, meter, footer; icon set
- [ ] Scheduler: 1-minute redraw tick, 30/15-minute fetch tick, 6-hour NTP tick
- [ ] 72-hour soak on USB with serial logging; no reboot, no heap creep (AV-002)
**Acceptance:** All Must features pass their acceptance criteria; soak log clean; a photograph of the panel matching the `design/` mockup within reason.

## Phase 2 — Resilience and pages
**Goal:** Behave well when things go wrong, and give the user more than one page.
**Status:** Not started
**Features delivered:** F-012, F-013, F-015
**Deliverables:**
- [ ] Boot splash with self-test checklist
- [ ] KEY button page cycling with 60 s return
- [ ] Diagnostics page
- [ ] Footer one-liner table
- [ ] Fault injection: pull Wi-Fi, block DNS, feed malformed JSON, unplug USB mid-fetch — all recorded in BUGS.md or confirmed handled
**Acceptance:** Every AV- entry marked Critical has implemented detection; the fault-injection list passes.

## Phase 3 — Battery and night
**Goal:** Run usefully off the 18650 for at least a week between charges.
**Status:** Not started
**Features delivered:** F-014; the deep-sleep candidate feature promoted if viable
**Deliverables:**
- [ ] Measured power budget per state (SPEC.md §Power) and a projected runtime
- [ ] Night mode layout and schedule
- [ ] Decision on light sleep vs deep sleep between fetches (new D- entry; depends on AV-006)
- [ ] Low-battery warning in footer and graceful shutdown threshold
**Acceptance:** ≥ 7 days measured runtime from full charge with the default schedule, or a D- entry recording why that target was revised.

## Phase 4 — The object
**Goal:** Enclosure, filter, finish.
**Status:** Not started
**Features delivered:** F-018
**Deliverables:**
- [ ] Gel swatch test on the live panel; record the chosen gel in D-012
- [ ] Case CAD in `enclosure/`, printed and fitted; bezel, screws, button caps, grille
- [ ] Badge/label artwork
- [ ] Photographs into `design/` and the README
**Acceptance:** The device sits on the wall or desk looking like the `design/` enclosure render, with every port and button usable.

## Later / unscheduled
- F-016 seconds clock (mains-only build flag)
- F-017 spoken advisory
- Candidate features listed in FEATURES.md
