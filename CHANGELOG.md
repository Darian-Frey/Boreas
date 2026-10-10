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
