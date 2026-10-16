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
**Status:** In progress. All firmware for the Main page is done and running on the board (2026-10-16). Remaining: an uninterrupted 72 h soak clear of BUG-004, the RTC backup cell for F-009, CI, and the photo check.
**Features delivered:** F-001, F-002, F-003, F-004, F-005, F-006, F-007, F-008, F-009, F-010, F-011
**Deliverables:**
- [x] `src/display/` — `St7305Panel`: init, `canvas()`/`raw()`, `end_frame()` with the 100 ms HPM hold; on the board 2026-10-10 (21 ms per frame). `reinit()` self-heal every 15 min added 2026-10-16 (D-019, BUG-004 mitigation)
- [x] `src/net/` — Wi-Fi station with capped-backoff reconnect; NTP + TZ; RTC write with the D-015 marker; `MeteoClient` with verified TLS (D-017). Done 2026-10-10; Wi-Fi fail-fast on driver disconnect 2026-10-16 (IMP-004: up at ~8 s after a reset instead of ~21 s)
- [x] `src/data/` — `WeatherModel`, `PanelState`, `Derived` (dew point, Δ, comfort, WMO text and icons, outlook, umbrella, trace extremes incl. the low, stale flag), `PressureHistory`; Open-Meteo JSON filter and AV-009 validation in `net/MeteoParse.h`. Done 2026-10-10/16
- [x] `src/sensors/` — SHTC3, battery ADC, RTC (D-015 validity rule); host tests for date maths and gauge. Done 2026-10-10
- [x] `src/ui/` — frame, three columns, trace (peak and low), rain meter, footer, drawn icons (D-018); geometry tests in `test/test_layout`. Main page complete 2026-10-10; low label 2026-10-16; fonts restyled with U8g2 built-ins 2026-10-16 (IMP-001: VCR clock, spleen labels/readouts)
- [x] Host preview build (D-016): `ui/` + U8g2 C core → PNG, hash identical on host and board (`025ad6cf`); fixtures `typical`/`worst`/`dry`/`stale`/`unset`; font sheets. `design/panel-reference.png` regenerated with every `ui/` change (currently `d09885fa`; BUG-001 fixed)
- [x] Scheduler — `app/Schedule` timing rules (fetch 30/15 min on slots 7 min 15 s past, IMP-005; retry backoff; redraw on the minute and on change; 13 host tests, mutation-checked) and `app/App`, the single `PanelState` owner (2026-10-11). 6 h NTP is lwIP SNTP's own interval
- [x] Health log on the TF card — `storage/SdLog`, `/boreas/health.csv` (boots with reset reason, 10 min ticks, fetch failures); `dump` serial command + `tools/sdlog.py` read it over USB; `tools/soak.py` judges a soak (2026-10-11/16). TF pins verified (SDMMC 1-bit)
- [ ] 72-hour soak on USB; no reboot, no heap creep (AV-002), no stuck panel (BUG-004). First soak 2026-10-11 → 16 (116 h, `INCOMPLETE`): heap trend −66 to −84 B/day, largest block constant at 131 060, no crashes, 0 Wi-Fi reconnects, NTP every ~6 h, 93 % fetches OK (all failures Open-Meteo 503s on the hour → IMP-005). Two power-offs to recover a blank and a half-blank panel split it into 19.5 h, 41.8 h and 54.5 h runs. **Next:** one uninterrupted 72 h run on the current build (D-019 self-heal, IMP-004, IMP-005, new fonts)
- [ ] BUG-004 (panel stops updating) — mitigated by D-019; close once a 72 h soak passes with no stuck panel, or pursue the cause (HPM-only and SD-off variants) if it recurs
- [ ] F-009 power-cycle criterion — fit an MS621FE rechargeable cell to the RTC's PH1.0 header. Without it the RTC loses time at every power-off (seen at each soak power-on), so the clock waits for NTP
- [ ] CI — GitHub Actions on every push: firmware build (example `secrets.h`/`locations.h`), `pio test -e native`, preview build with the reference-hash check, no `String`/`new` under `src/ui/`. AV-001 and AV-002 already rely on it
- [ ] Photograph the panel and compare with `design/panel-reference.png`
**Acceptance:** All Must features pass their acceptance criteria; soak log clean; a photograph of the panel matching `design/panel-reference.png` (generated, D-016) within reason.

## Phase 2 — Resilience and pages
**Goal:** Behave well when things go wrong, and give the user more than one page.
**Status:** Not started
**Features delivered:** F-012, F-013, F-015
**Deliverables:**
- [ ] Boot splash with self-test checklist
- [ ] KEY button page cycling with 60 s return (under consideration 2026-10-16: an add-on 4-button module, K1–K4 switching to GND, plugged into header P1 pins 3–11 for GPIO0–3; functions and pins not yet decided)
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
