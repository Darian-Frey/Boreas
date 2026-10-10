# Decisions

Append-only log of significant design decisions.
Each entry: D-NNN, with Decided and Recorded dates (ISO 8601; equal for normal entries, divergent for retroactive ones), status, context, alternatives, decision, consequences, and reversal conditions.
Status vocabulary: Proposed | Accepted | Superseded by D-NNN | Deprecated.

### D-001 Target the Waveshare ESP32-S3-RLCD-4.2 instead of the Pi Zero WH + Inky pHAT
**Decided:** 2026-10-06
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (with Claude planning discussion 2026-10-06)
**Related:** F-001, F-003, F-011, AV-006, SPEC.md §Hardware

**Context.** The project started as a way to use an idle Pi Zero WH and Inky pHAT (250×122 1-bit e-ink). The pHAT's resolution forced cuts to the readout set, the Pi's Pwnagotchi image put wlan0 in monitor mode and would have needed a fresh card anyway, and a wall display wants low idle power and a battery option.

**Options.**
- **A. Pi Zero WH + Inky pHAT, fresh Pi OS Lite.** Python/Pillow rendering, easy iteration. Rejected: 250×122 is too small for the indoor/outdoor split; ~0.5 W idle; SD card and full OS for a clock.
- **B. Waveshare ESP32-S3-RLCD-4.2.** 300×400 1-bit reflective LCD, SHTC3 indoor sensor, RTC, 18650 holder, ~£25. Chosen.
- **C. A true e-ink 4.2" ESP32 board.** Better contrast and image retention without power. Rejected for this version: flashing full refresh, no live clock, no indoor sensor on the common boards.

**Decision.** Option B. The Pi + pHAT are kept as a possible smaller sibling (FEATURES.md candidate list).

**Consequences.**
- Firmware is C++ on the Arduino framework rather than Python; iteration is slower, so a host-side preview tool is worth building (`tools/preview.py`).
- Indoor readings become a first-class panel column (F-003).
- The panel holds its image only while powered; a dead battery means a blank screen (AV-006).

**Reversal conditions.** Revisit if the ST7305's contrast under an amber gel proves unreadable in normal room light (Phase 4 gel test), or if measured battery life in Phase 3 is under 3 days with no viable sleep strategy.

### D-002 Arduino framework via PlatformIO with the pioarduino platform
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (proposed by Claude 2026-10-10)
**Related:** BUILD.md, AV-001

**Context.** The U8g2 ST7305 driver needs arduino-esp32 core ≥ 3.3.0. The official PlatformIO `espressif32` platform is pinned to the 2.x core; the community **pioarduino** fork tracks 3.x. Waveshare's examples are written for the Arduino IDE.

**Options.**
- **A. Arduino IDE 2.x.** Matches Waveshare's docs exactly. Rejected as the primary path: GUI-only builds don't suit a scripted, AI-assisted workflow; library versions are global rather than per-project.
- **B. PlatformIO Core + pioarduino.** Per-project `platformio.ini` pins the core and every library; CLI build/upload/monitor; unit tests on host. Chosen.
- **C. Native ESP-IDF.** Lowest-level control, best sleep/power tooling. Rejected for now: U8g2 and the Waveshare examples are Arduino-first; the IDF learning cost isn't justified by Phase 1–2 needs.

**Decision.** Option B, with the Arduino IDE kept as a documented fallback in BUILD.md for running Waveshare's stock demos unmodified.

**Consequences.**
- `platformio.ini` is the single source of truth for versions (core, U8g2, ArduinoJson, SensorLib).
- pioarduino is a community platform; its URL and release tag are pinned in `platformio.ini` and BUILD.md.

**Reversal conditions.** Move to C if Phase 3 power work needs IDF-only APIs that the Arduino core doesn't expose cleanly (e.g. fine control of light sleep with Wi-Fi), or if pioarduino stops tracking the core.

### D-003 Single-day forecast, Celsius only
**Decided:** 2026-09-25
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** F-002, F-005, F-006

**Context.** The brief is a glanceable "what's today like" panel, not a forecasting tool.

**Options.**
- **A. Today only.** Chosen. Leaves room for the 12-hour trace and indoor column.
- **B. Three-day strip.** Rejected: crowds the panel; the second and third days are rarely what you need from a wall glance.

**Decision.** Today only; the Detail page (F-013) shows the full 24 h hourly table rather than more days.

**Consequences.** Open-Meteo request is small (one daily row). Night mode (F-014) may show "tomorrow's outlook" as its one exception.

**Reversal conditions.** If the Detail page proves more used than Main, reconsider a multi-day page.

### D-004 Open-Meteo as the forecast source
**Decided:** 2026-09-25
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (proposed by Claude)
**Related:** F-008, AV-003, SPEC.md §Open-Meteo request

**Context.** Needs a free, keyless, JSON forecast API with hourly resolution for a UK location.

**Options.**
- **A. Open-Meteo.** Free for non-commercial use, no key, hourly/daily fields, UK Met Office model available via the `models` parameter. Chosen.
- **B. Met Office DataHub.** Authoritative for the UK but needs an API key and has a more involved auth flow.
- **C. OpenWeatherMap.** Key required; free tier limits and licensing churn.

**Decision.** Option A. Default model selection left to Open-Meteo's `best_match`; `ukmo_seamless` noted as an option in SPEC.md.

**Consequences.** No secrets beyond Wi-Fi. The request URL is long; built once at boot. Rate limits are generous but AV-003 guards against hammering on retry.

**Reversal conditions.** Open-Meteo changes licensing, removes the keyless tier, or the forecast quality for Hertfordshire is noticeably poor against the Met Office app over a month of use.

### D-005 Render with U8g2 full-buffer mode, not LVGL
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (proposed by Claude)
**Related:** F-001, AV-002, SPEC.md §Panel

**Context.** Waveshare ships LVGL v8 and v9 examples and a U8g2 example for this board. The panel is 1-bit; the design is fixed-layout, no widgets or touch.

**Options.**
- **A. LVGL.** Rich widgets, animation, theming. Rejected: heavyweight for a static instrument panel; its 1-bit support is an afterthought; adds a large dependency and config surface.
- **B. U8g2 full buffer (`_F_` constructor).** Native ST7305 300×400 support since 2.36.19, 15 000-byte buffer, dozens of bitmap fonts, XBM icons, primitives. Chosen.
- **C. Hand-rolled framebuffer + own font rasteriser.** Maximum control, matches the "Pillow on the Pi" plan. Rejected: U8g2 already gives this with fonts for free.

**Decision.** Option B. All drawing goes through one `Layout` module so a later swap is contained.

**Consequences.**
- The 15 000-byte buffer lives in static RAM (fine on the S3's 512 KB).
- Fonts are limited to U8g2's set; the cassette-futurism look comes from font choice and drawn chrome rather than custom typefaces. Custom fonts can be converted with `bdfconv` if needed (IMP candidate).
- `tools/preview.py` must replicate the grid, not the fonts, so host previews are approximate.

**Reversal conditions.** If the UI needs animation beyond the VU meter and clock, or if U8g2's ST7305 driver proves to mishandle the LPM/HPM sequence (AV-006), reconsider A or C.

### D-006 Panel refresh strategy: redraw per minute, LPM between writes
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (proposed by Claude)
**Related:** F-011, F-016, D-008, SPEC.md §Panel modes

**Context.** The ST7305 self-refreshes at 32 Hz in High Power Mode and 1 Hz in Low Power Mode. Writing in LPM delays the visible update by up to a second; switching to HPM for the write then back avoids that.

**Options.**
- **A. Stay in HPM always.** Simplest; needed for a seconds clock. Rejected as default: wastes panel power for 59 s of every minute.
- **B. HPM for write, LPM after, redraw once a minute.** Chosen.
- **C. Redraw only on data change.** Lowest write count. Rejected: the minute clock wants a redraw anyway, and a periodic redraw masks any missed-update bugs.

**Decision.** Option B. `St7305Panel::endFrame()` does `0x38` → send buffer → wait → `0x39`.

**Consequences.** The seconds clock (F-016) is mains-only behind a build flag that forces option A.

**Reversal conditions.** If measured LPM panel current is not meaningfully below HPM (Phase 0 measurement), drop to A and simplify.

### D-007 Time source: NTP primary, PCF85063 RTC secondary
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (proposed by Claude)
**Related:** F-009, AV-005; validity rule refined by D-015

**Context.** The board has a PCF85063 with a backup-cell header. The clock must be right after a power cycle even with no Wi-Fi.

**Decision.** SNTP with TZ rule `GMT0BST,M3.5.0/1,M10.5.0` on boot and every 6 h; after each sync, write the RTC. On boot, read the RTC first so the panel shows a correct time before Wi-Fi is up. `RTC OK` only when the chip's oscillator-stop flag is clear.

**Options considered.** RTC-only with manual setting (rejected: no button UI for it); NTP-only (rejected: wrong time for the first seconds after every boot and indefinitely without Wi-Fi).

**Consequences.** Fit a PH1.0 rechargeable RTC cell (MS621FE or equivalent) in Phase 0; without it the RTC resets on every battery swap.

**Reversal conditions.** If the RTC drifts > 30 s/day (AV-005), re-sync more often or treat RTC as boot-only.

### D-008 Minute-resolution clock by default; seconds is a build flag
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** F-016, D-006

**Context.** The RLCD can show a live seconds clock, which the e-ink original could not — but it forces HPM permanently and a redraw every second.

**Decision.** Default build shows HH:MM. `BOREAS_SECONDS_CLOCK=1` enables HH:MM:SS, forces HPM and a 1 s redraw, intended for mains-powered desk use.

**Consequences.** Two code paths in `Footer`/`Frame` and in the scheduler; both must be kept working (test both in Phase 2).

**Reversal conditions.** If Phase 3 shows the panel's HPM cost is negligible against Wi-Fi, make seconds the default.

### D-009 Fixed layout grid, landscape 400×300 via `U8G2_R1`
**Decided:** 2026-10-06
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** F-001, SPEC.md §Layout

**Context.** The native panel is 300×400 portrait; the instrument-panel design is landscape.

**Decision.** Rotate in the driver (`U8G2_R1`) and lay out on a fixed pixel grid defined once in `ui/grid.h`. No runtime layout computation.

**Consequences.** The enclosure (F-018) is landscape; USB-C ends up on the side it ends up on.

**Reversal conditions.** None expected; a portrait variant would be a separate layout module.

### D-010 Wi-Fi credentials compiled in from a git-ignored header
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (proposed by Claude)
**Related:** F-010, BUILD.md

**Context.** Single user, single network. A captive-portal provisioning flow is a feature in itself.

**Decision.** `include/secrets.h` (ignored) from `secrets.example.h`. No runtime provisioning in this version.

**Consequences.** Changing networks means a reflash. Diagnostics page shows the SSID so a wrong build is obvious.

**Reversal conditions.** If a second unit is built for someone else, add WiFiManager-style provisioning and record it as a new decision.

### D-011 HTTPS to Open-Meteo without certificate verification
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Proposed
**Authors:** Claude (proposal; awaiting Shane's adjudication)
**Related:** F-008, AV-003, AV-009

**Context.** Open-Meteo is HTTPS-only. arduino-esp32 can verify against a pinned root CA or the built-in bundle, or skip verification with `setInsecure()`.

**Options.**
- **A. `setInsecure()`.** No cert maintenance; the data is public weather and there are no credentials in the exchange. The risk is a spoofed forecast on a hostile network.
- **B. Pin the root CA.** Correct, but a CA rotation bricks the fetch until reflash.
- **C. Use the core's certificate bundle.** Best of both; costs ~60 KB flash and a build step.

**Decision (proposed).** Start with A in Phase 1 to unblock; move to C in Phase 2 and record the change as a new entry.

**Reversal conditions.** Adopt C immediately if the device is ever put on a network Shane doesn't control.

### D-012 Amber gel over the panel — choice deferred to a swatch test
**Decided:** 2026-10-06
**Recorded:** 2026-10-10
**Status:** Proposed
**Authors:** Shane Hartley
**Related:** F-018, D-001 reversal conditions

**Context.** The panel is mono; the cassette-futurism colour comes from a lighting gel over it and the enclosure. Reflective displays pass light through the gel twice, so saturated gels darken more than expected.

**Options.** Lee 158 Deep Orange (classic phosphor amber, risk of crushed contrast); Lee 020 Medium Amber; Lee 147 Apricot / 204 Full CTO (subtle); Lee 139 Primary Green (terminal look).

**Decision (proposed).** Buy a Lee swatch book, test on the live panel in Phase 4, record the winner by amending this entry's status and adding the result to SPEC.md §Enclosure.

**Reversal conditions.** If every gel makes the panel unreadable in evening room light, drop the gel and carry the colour entirely in the case.

### D-013 Tier 3 exemptions: no CLAIMS.md, no VOCABULARY.md, no BENCHMARKS.md
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (recorded by Claude)

**Context.** The documentation standard requires exemptions to be recorded.

**Decision.** `CLAIMS.md` is not created: the project makes no research assertions. `VOCABULARY.md` is not created: there is no sibling project sharing a contract (if the Pi/Inky sibling is built, revisit). `BENCHMARKS.md` is not created: the only performance figures are power measurements, which live in SPEC.md §Power. `TESTING.md` is deferred until the test set spans more than PlatformIO's native and embedded runners.

**Reversal conditions.** Any of the named trigger conditions firing.

### D-014 MIT licence
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Proposed
**Authors:** Claude (proposal; awaiting Shane's adjudication)

**Context.** Tier 1 requires a licence before first public commit. Shane's other repos should set the convention; none was specified for this one.

**Decision (proposed).** MIT, matching the permissive norm for hobby firmware and the U8g2 (BSD) / ArduinoJson (MIT) dependencies. Replace the LICENSE file and this entry if a different licence is preferred.

### D-015 RTC time is trusted only after NTP has set it
**Decided:** 2026-10-10
**Recorded:** 2026-10-10
**Status:** Accepted
**Authors:** Shane Hartley (options proposed by Claude)
**Related:** D-007, F-009, AV-005, BUG-002, SPEC.md §RTC

**Context.** D-007 reports `RTC OK` when the PCF85063's oscillator-stop (OS) flag is clear. In Phase 0 the new board read `2026-01-05` on 2026-10-10 with the flag clear (BUG-002), so that rule would show a confident, wrong clock on any boot without Wi-Fi.

**Options.**
- **A. OS flag only** (D-007 as written). Rejected: it passes a running but never-set clock, as BUG-002 showed.
- **B. OS flag plus a year/date plausibility check** (e.g. not earlier than the firmware build date). Rejected as the rule: a stale but plausible time still passes.
- **C. OS flag plus an "NTP-set" marker** in the PCF85063's free RAM byte (register `0x03`), written only by `NtpSync` after it writes the time. Chosen.

**Decision.** Option C. The RTC is valid only when the OS flag is clear **and** register `0x03` holds `kRtcNtpMarker`. `NtpSync` writes the time (UTC), then the marker, after every successful sync. Anything else that writes the clock, such as the bring-up test, leaves the marker unset, so the firmware treats that time as unknown.

**Consequences.**
- The footer shows `RTC OK` only for a time NTP has set. Without that, the header clock shows `--:--`, the footer shows `RTC UNSET`, and the panel waits for NTP.
- Losing RTC power (battery swap or flat cell with no backup cell) resets the RAM byte and sets the OS flag. Either one invalidates the time, so a power loss can't leave a stale time marked trusted.
- The marker register was verified on the board in Phase 0: writes and reads back, original restored.

**Reversal conditions.** If the RAM byte proves unreliable (e.g. cleared by something other than power loss), fall back to B as a secondary check alongside the OS flag.
