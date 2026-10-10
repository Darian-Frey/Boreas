# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com). Reference F-, D-, AV-, BUG- and IMP- IDs.

## [Unreleased]
### Added
- Documentation scaffolding per the development documentation standard: README, FEATURES (F-001–F-018), ROADMAP (Phases 0–4), ARCHITECTURE, DECISIONS (D-001–D-014), SPEC, ATTACK_VECTORS (AV-001–AV-010), BUGS, IMPROVEMENTS (IMP-001–IMP-003), BUILD, CLAUDE, LICENSE.
- Panel mockup at 4× scale (`design/panel-mockup.png`); known to disagree with SPEC.md §Layout (BUG-001).
- `platformio.ini` (pioarduino, ESP32-S3 N16R8, `boreas` and `bringup` environments), `include/pins.h`, `include/secrets.example.h`.
- Phase 0 bring-up test (`test/bringup/`): I2C scan, PCF85063 and SHTC3 over raw I2C, battery ADC, button press logging, U8g2 test card with tunable HPM hold, frame timing and loop-gap reporting.
- AV-011: serial logging stalls the loop when no host reads the USB CDC port.
- BUG-002: oscillator-stop flag clear does not prove the RTC time is valid.
- D-015: RTC time is trusted only when NTP has set it (NTP marker in PCF85063 register 0x03); closes BUG-002.
- `src/display/St7305Panel` (Phase 1): owns the U8g2 ST7305 driver and the HPM → LPM write sequence; `src/main.cpp` scaffold redraws a placeholder every 60 s.
- `src/sensors/` readers: `Shtc3`, `Battery`, `Rtc` (D-015 validity rule, NTP-only marker writer), `CivilTime.h`; `src/data/Models.h`; `include/config.h`.
- `[env:native]` with host unit tests (`test/test_sensors`: date conversion 2000–2099, battery gauge); `default_envs = boreas`.
- BUG-003: SPEC's USB-present rule could never fire (4.12–4.14 V measured on USB). Fixed by dropping the indicator; VBUS sensing noted for Phase 3.
- D-016: the host preview renders the real `ui/` code with U8g2 instead of a Python approximation; IMP-003 declined as superseded.
- `src/net/`: `WiFiMgr` (polled, capped backoff 5 s → 5 min), `NtpSync` (SNTP every 6 h, writes RTC + NTP marker, logs drift), `Backoff` (shared with future fetch retries); host tests in `test/test_net`. Scaffold sets the system clock from a trusted RTC at boot (D-007).
- IMP-004: fail Wi-Fi attempts as soon as the driver reports failure.
- `MeteoClient` and `MeteoParse.h`: Open-Meteo fetch with `timeformat=unixtime` and `forecast_hours=24`, filtered parse, AV-009 validation that keeps the last good model on failure; `WeatherModel`; host tests with a real response fixture (`test/test_meteo`).
- F-008 memory criterion reworded: the 40 KB limit applies to the JSON parse. The whole verified-TLS fetch peaks at about 55 KB, recorded in SPEC.md, and must return heap to its pre-fetch level.
- D-017: TLS verified with the core's built-in CA bundle from the first fetch; supersedes D-011.
- `src/data/Derived` and `PressureHistory`: dew point, Δ, comfort, meters, WMO text and icons with night variants, compass, UV band, 12 h outlook and umbrella rule, trace extremes, stale flag; time-based 3 h pressure trend instead of the 6-slot ring. Host tests in `test/test_derived`; native env now compiles `src/data/`.
- Host preview harness (D-016): `display/MemCanvas` (RAM canvas on U8g2's C core, pixel hash), `ui/TestCard`, `tools/preview` (scene → PNG, no image library), `[env:preview]`. `St7305Panel::raw()` exposes the C API. Host and board hashes match (`025ad6cf`).
- `ui/grid.h`, `ui/Fonts.h`, `ui/Draw.h`, `ui/Frame` (border, header with location/date/clock, rules, tabs) and `data/PanelState.h`; preview fixtures and a `frame` scene; geometry tests in `test/test_layout`. SPEC.md §Layout: footer moved to y = 280 inside the border, strip 79 px, tabs inset 1 px, source labels in 5×7, font table.
- F-002 reworded: feels/high/low stacked beside the icon instead of one line, which can't fit the column.
- `ui/Icons` (11 condition icons from primitives, D-018) and `ui/Outdoor` (EXT column, F-002); hero font `logisoso50_tn` to meet F-002's 48 px. Preview scenes `main`, `icons`, `feels-oneline`; fixtures carry weather data. The scaffold now draws the live Main page instead of a debug card.
- `ui/Indoor` (INT column, F-003): temperature, humidity, 10-cell RH meter with the 30–60 % band bracket, Δ in/out and dew point, comfort box (inverted when out of band). Δ and arrow symbols moved to `ui/Icons` (test-card hash unchanged).
- `ui/Readouts` (READOUTS column, F-004): wind, gust, humidity, pressure with a drawn trend arrow, UV band, sunrise and sunset. `DerivedModel::today` picks the current local day's row (also used for night detection). Preview fixtures carry pressure history.
- `include/locations.example.h`: forecast location template. The real location lives in the git-ignored `include/locations.h`.
### Changed
- Project retargeted from Raspberry Pi Zero WH + Inky pHAT to the Waveshare ESP32-S3-RLCD-4.2 (D-001).
- Project named Boreas MS-42.
- U8g2 pinned to a GitHub commit (2.37.1); the PlatformIO registry stops at 2.36.18, before the ST7305 300×400 driver (AV-001).
- SPEC.md: pin map rebuilt from the Waveshare schematic; expansion header P1 documented; GPIO17 reserved for an add-on location button; I2C scan, panel driver, and the 100 ms HPM hold verified on the board.
- Platform and libraries pinned exactly (IMP-002): pioarduino 55.03.312, ArduinoJson 7.4.3.
- Phase 0 (board bring-up) complete. Current measurements deferred to Phase 3; the location button deferred as a nice-to-have.
### Fixed
- README clone URL and folder name now match the `Darian-Frey/Boreas` repository.
