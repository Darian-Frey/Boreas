# Roadmap

Phases are append-only. Mark Complete with an ISO date.

## Phase 0 — Board bring-up
**Goal:** Prove the brand-new board, the host toolchain and every peripheral the project needs, before writing any Boreas code.
**Status:** Complete 2026-10-10. Acceptance met. Deferred items are marked below and carried into Phase 3 (power) or left as nice-to-haves.
**Features delivered:** (none — this phase establishes the build path in BUILD.md and seeds ATTACK_VECTORS.md)
**Deliverables:**
- [x] Unbox, fit 18650, first power-on (2026-10-10). The factory firmware behaviour was not noted before it was overwritten; re-flash Waveshare's demo if a baseline is ever needed
- [x] Linux host sees the board on USB-C (`/dev/ttyACM0`), user in `dialout`, `brltty` conflict ruled out (2026-10-10; board must be powered on with PWR before it enumerates)
- [x] PlatformIO + pioarduino environment compiles and uploads (2026-10-10, `test/bringup` in place of a blink; esptool confirms ESP32-S3 rev 0.2, 16 MB flash, 8 MB PSRAM)
- [x] Waveshare `05_I2C_SHTC3` and `04_I2C_PCF85063` ported into `test/bringup/` and verified on serial (2026-10-10; raw I2C; RTC validity caveat in BUG-002)
- [x] U8g2 ST7305 test: full-buffer draw at 400×300 landscape, HPM→LPM switch observed (2026-10-10: orientation, corners, 1 px lines and fonts correct; 100 ms HPM hold gives instant updates; frame send 21 ms)
- [x] `03_ADC_Test` equivalent: battery voltage reads plausibly with and without USB (2026-10-10: 4.12–4.14 V on USB while charging, 3.97 V on battery; 3:1 divider still unconfirmed against a multimeter)
- [ ] Button test: BOOT (GPIO0), KEY (GPIO18) and the add-on location button (GPIO17, header P1-13, if fitted) each log press and release on serial, with no spurious or bouncing events (SPEC.md §Pin map). BOOT and KEY passed 2026-10-10 (no bounce, no missed presses after the AV-011 fix). **Deferred:** the location button is a nice-to-have, not yet fitted
- [x] I2C scan of GPIO13/14 lists 0x51 (RTC) and 0x70 (SHTC3); note any other addresses, e.g. a QMI8658C, in SPEC.md (2026-10-10: also 0x18, 0x40; no QMI8658C)
- [ ] Measured current draw: idle in LPM, during fetch, during redraw — recorded in SPEC.md §Power. **Deferred to Phase 3:** no USB current meter yet; needs an inline USB meter or a multimeter in series with the cell
**Acceptance:** `pio run -t upload` of `test/bringup` shows all peripherals passing on serial and a test pattern held on the panel in LPM.

## Phase 1 — Live panel (mains)
**Goal:** The full Main page on the panel, fed by real data, running indefinitely on USB power.
**Status:** In progress — `src/display/`, the `src/sensors/` readers, and Wi-Fi/NTP in `src/net/` done 2026-10-10
**Features delivered:** F-001, F-002, F-003, F-004, F-005, F-006, F-007, F-008, F-009, F-010, F-011
**Deliverables:**
- [x] `src/display/` — `St7305Panel`: init, `canvas()`, `end_frame()` with the 100 ms HPM hold; on the board 2026-10-10 (21 ms per frame, 60 s redraw)
- [ ] `src/net/` — Wi-Fi station with reconnect; NTP + TZ; RTC sync. Wi-Fi + NTP + RTC sync done 2026-10-10 (on the board: connect 5 s, sync 3 s later, RTC trusted at next boot, recovered from a refused association). `MeteoClient` done 2026-10-10 (verified TLS, 0.7–0.9 s per fetch, retries on the shared `Backoff`)
- [ ] `src/data/` — Open-Meteo client with JSON filter; `WeatherModel` struct; stale/retry logic. Client, filter, validation and `WeatherModel` done 2026-10-10; retry runs in the scaffold. Stale flag and `app/Scheduler` still to do
- [x] `src/sensors/` — SHTC3 reader, derived values (dew point, Δ, comfort band); battery ADC. Readers done 2026-10-10 (SHTC3, battery, RTC with the D-015 rule; host tests for date maths and gauge). Derived values done 2026-10-10 in `src/data/Derived` + `PressureHistory` (20 host tests; checked against the live forecast on the board)
- [ ] `src/ui/` — layout engine on U8g2: frame, columns, trace, meter, footer; icon set
- [ ] Host preview build (D-016): `ui/` + U8g2 C core → PNG from fake `PanelState` fixtures, including AV-007 worst cases; generated reference committed to `design/` (closes BUG-001). Harness done 2026-10-10: `pio run -e preview`, test-card scene, hash identical on host and board (`025ad6cf`). Fixtures and the reference image come with the layout
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
- [ ] If power policy needs to know about mains (e.g. faster fetches on USB): add VBUS sensing with a 100 kΩ/100 kΩ divider from header P1-2 to GPIO1 (P1-7, ADC1). Firmware can't detect USB without it (BUG-003)
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
