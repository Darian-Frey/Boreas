# CLAUDE.md

## Project

Boreas MS-42: a cassette-futurism weather station on the Waveshare ESP32-S3-RLCD-4.2 (ESP32-S3, 300×400 1-bit reflective LCD, SHTC3, PCF85063 RTC, 18650). Single-day Celsius forecast from Open-Meteo plus live indoor temperature/humidity, drawn as an instrument panel with U8g2. Read [DECISIONS.md](DECISIONS.md) before proposing structural changes; read [SPEC.md](SPEC.md) for every constant.

## Current state

- **No firmware exists yet.** The repository holds documentation and the design mockups only.
- `design/` — `panel-mockup.png` (4× scale; known to disagree with SPEC.md §Layout, BUG-001, so SPEC wins). Enclosure concept not yet added.
- `platformio.ini` — not yet created; reference content is in BUILD.md §3.3.
- Hardware — board ordered/arriving; Phase 0 bring-up not started.
- SPEC.md carries **[verify]** tags on the battery divider ratio, TF card CS pin, QMI8658C presence, U8g2 constructor name and LPM delay. Strike each only after checking on the board or the Waveshare schematic.

## Active task

**Phase 0 — board bring-up** (ROADMAP.md). Next concrete item: create `platformio.ini` from BUILD.md §3.3 and a `test/bringup/main.cpp` that (1) prints RTC time, (2) prints SHTC3 T/RH, (3) prints battery voltage, (4) scans I2C and logs BOOT/KEY/location-button presses, (5) draws a test card with U8g2 and drops the panel to LPM. Acceptance is in ROADMAP.md Phase 0. Record measured currents in SPEC.md §Power.

## Invariants

- `ui/` reads `PanelState` and writes pixels; it never calls network, sensor or time functions.
- Exactly one `PanelState`, owned by `app/`.
- No heap allocation in the redraw path (AV-002). No `String` under `src/ui/`.
- Every GPIO is named in `include/pins.h`; no numeric pin literals elsewhere.
- The panel is never blanked after boot; failures keep the last good frame and set a flag.
- Every network operation has an explicit timeout.
- `St7305Panel::endFrame()` is the only place that sends `0x38`/`0x39`.

## Build & test

```bash
pio run -t upload && pio device monitor          # flash + serial
pio run -e bringup -t upload                     # Phase 0 peripheral test
pio test -e native                               # host-side unit tests
python3 tools/preview.py --fake > /tmp/panel.png # layout without hardware
```

## Conventions

- British English in docs, comments and UI strings (`COLOUR` never appears on the panel, but `HUMID`, `NOMINAL` etc. follow the mockup's caps style).
- C++17, `-Wall -Wextra`, no exceptions, no RTTI. `snake_case` functions and variables, `PascalCase` types, `kConstant` for constexpr, `UPPER_SNAKE` only for build flags.
- One module per directory under `src/` with a `.h`/`.cpp` pair; headers include only what they use.
- Commit messages: multi-paragraph, imperative subject ≤ 72 chars, body explains why; reference IDs (`F-003`, `D-006`, `AV-002`, `BUG-`, `IMP-`).
- Docs are part of the commit: a change that invalidates SPEC.md/ARCHITECTURE.md updates them in the same commit.
- **Log, don't silently fix** (Maintenance Rule 8): bugs found while doing something else go in BUGS.md; refactor ideas go in IMPROVEMENTS.md with trade-offs. Ask before acting on either.

## Pitfalls

- See ATTACK_VECTORS.md for the canonical list. Most likely to bite first: AV-001 (wrong platform/core silently gives a blank panel), AV-006 (writing in LPM looks like a one-second lag), AV-004 (indoor temp reads high from board heat).
- `SPI.begin(11, -1, 12, 40)` must precede `u8g2.begin()`; U8g2's constructor pins alone are not enough on this core.
- The panel does not retain an image without power. A "blank screen" bug report may just be a flat cell.
- Waveshare's Arduino examples assume LVGL 8 *or* 9 installed globally — never both. Irrelevant to the PlatformIO build but a trap when running their demos.
- Cross-references between docs are maintained by hand; there is no `check_xrefs` tool yet, so treat them as possibly stale.

## Out of scope

Do not, without asking:
- Switch rendering library (D-005), framework/platform (D-002), or forecast provider (D-004).
- Add runtime Wi-Fi provisioning (D-010), a web UI, MQTT, or voice features (F-017 is Won't).
- Change the layout grid in `ui/grid.h` or the fonts without updating SPEC.md §Layout and the design export.
- Enable `setInsecure()` beyond what D-011 allows, or adopt certificate pinning (D-011 names the intended path).
- Create new document types or renumber any F-/D-/AV-/BUG-/IMP- IDs.
