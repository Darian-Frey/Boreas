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
### Changed
- Project retargeted from Raspberry Pi Zero WH + Inky pHAT to the Waveshare ESP32-S3-RLCD-4.2 (D-001).
- Project named Boreas MS-42.
- U8g2 pinned to a GitHub commit (2.37.1); the PlatformIO registry stops at 2.36.18, before the ST7305 300×400 driver (AV-001).
- SPEC.md: pin map rebuilt from the Waveshare schematic; expansion header P1 documented; GPIO17 reserved for an add-on location button; I2C scan, panel driver, and the 100 ms HPM hold verified on the board.
### Fixed
- README clone URL and folder name now match the `Darian-Frey/Boreas` repository.
