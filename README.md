> **Status:** Active
> **Provenance:** Shane Hartley (author, concept and design direction); Claude (documentation scaffolding and hardware research, 2026-10-10)
> **Last reviewed:** 2026-10-10
> **Why this status:** Initial scaffolding. Hardware ordered/arriving; Phase 0 (board bring-up) not yet started.

# Boreas MS-42

Boreas MS-42 is a cassette-futurism weather station for the wall or desk: a single-day Celsius forecast for Kings Langley alongside live indoor temperature and humidity, rendered as a 1-bit instrument panel on a 4.2" reflective LCD. It runs on the Waveshare ESP32-S3-RLCD-4.2 board (ESP32-S3, 300×400 ST7305 RLCD, SHTC3 sensor, PCF85063 RTC, 18650 cell) and fetches its forecast from Open-Meteo over Wi-Fi. The name is Boreas, the north wind; *MS* for Met Station; *42* for the 4.2" panel and for the answer to the ultimate question.

The project began as a Raspberry Pi Zero WH + Pimoroni Inky pHAT e-ink design; that lineage is preserved in [DECISIONS.md](DECISIONS.md) D-001. The physical object is intended to sit in the same product family as the Ferrolux RS-1.

## Quick Start

Assumes a Linux host with PlatformIO Core installed (see [BUILD.md](BUILD.md) for the blank-board path from unboxing).

```bash
git clone https://github.com/Darian-Frey/boreas-ms42.git
cd boreas-ms42
cp include/secrets.example.h include/secrets.h   # add Wi-Fi SSID/password
pio run -t upload                                  # board on USB-C; hold BOOT on first flash
pio device monitor -b 115200
```

The panel should show the boot splash, then the live panel within ~15 s of Wi-Fi association.

## Build requirements

- Linux (developed on Linux Mint); macOS/Windows untested
- PlatformIO Core ≥ 6.1 with the **pioarduino** platform (arduino-esp32 core ≥ 3.3.0 — required for the U8g2 ST7305 driver)
- U8g2 ≥ 2.36.19, ArduinoJson ≥ 7, SensorLib 0.3.1
- Python 3 (PlatformIO dependency; also used by `tools/`)

Full detail, including the first-power-on checklist for a brand-new board, in [BUILD.md](BUILD.md).

## Project structure

```
boreas-ms42/
├── src/                 # firmware (main.cpp + modules, see ARCHITECTURE.md)
├── include/             # headers, secrets.example.h, pins.h
├── lib/                 # vendored/local libraries (none yet)
├── assets/              # XBM icons, font notes, mockup exports
├── design/              # Design-canvas exports: panel mockup, enclosure concept
├── enclosure/           # case CAD (Phase 4)
├── tools/               # host-side helpers (icon converter, layout previewer)
├── test/                # PlatformIO unit tests (host-native where possible)
├── platformio.ini
└── *.md                 # the documents listed below
```

## Documentation

- [Features](FEATURES.md) — what the station does, with acceptance criteria
- [Roadmap](ROADMAP.md) — phased plan from bring-up to enclosure
- [Architecture](ARCHITECTURE.md) — firmware modules and data flow
- [Decisions](DECISIONS.md) — why it is built this way
- [Spec](SPEC.md) — pins, panel modes, layout grid, data contracts, constants
- [Attack vectors](ATTACK_VECTORS.md) — failure modes and how they are detected
- [Bugs](BUGS.md) / [Improvements](IMPROVEMENTS.md) — in-repo trackers
- [Build](BUILD.md) — toolchain, blank-board setup, flashing, troubleshooting
- [Changelog](CHANGELOG.md)
- [CLAUDE.md](CLAUDE.md) — handoff for AI-assisted sessions

## License

MIT — see [LICENSE](LICENSE).
