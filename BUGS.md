# Bugs

Catalogue of bugs discovered during development. Per Maintenance Rule 8,
bugs are logged here when found, not silently fixed. The author decides
whether to fix immediately, defer, or leave alone.

Status vocabulary: open | fixed | wontfix | deferred.
Severity vocabulary: low | medium | high.

## Open

### BUG-001: Panel mockup disagrees with SPEC.md §Layout and clips its column tabs
**Status:** open
**Found:** 2026-10-10 (filing the mockup into `design/`)
**Location:** `design/panel-mockup.png`
**Severity:** low
**Description.** The mockup was drawn in a browser with web fonts and doesn't follow the layout grid. SPEC.md §Layout is authoritative for now. While the mockup is wrong it can't serve as the Phase 1 photographic reference (ROADMAP Phase 1 acceptance, AV-007). Differences, measured in panel pixels (the image is 1600×1200, 4× native, despite the "2×" in its original filename):
- Header and column tabs merged into one black block at y 5–38. SPEC has a 20 px header (y 5–25) and separate 12 px tabs from y 26.
- Tab text `EXT · OUTDOOR` and `OPEN-METEO` wraps and is clipped at the bottom of that block.
- Column dividers at x 150 and x 270 (SPEC: 151 and 272).
- Bottom strip starts at y 192 (SPEC: 200); footer at y 280 (SPEC: 283).
- No divider between the temperature trace and the rain meter; the meter starts at about x 269 (SPEC: trace is 250 px wide).
- Outdoor hero temperature is about 34 px tall (F-002 requires at least 48 px).
- Header clock shows seconds (`13:30:07`); the default build is HH:MM (D-008).
- `FEELS · HI · LO` wraps onto two lines (F-002: one line).
**Reproduction.** Open `design/panel-mockup.png` beside SPEC.md §Layout.
**Notes.** Fix by redrawing the mockup on the SPEC grid, or replace it with a `tools/preview.py` render once that exists (IMP-003). If the redraw shows SPEC is the one that's wrong, update SPEC.md §Layout and close this entry.

## Fixed

_None._

## Won't Fix

_None._

## Deferred

_None._

---

Entry template:

```markdown
### BUG-001: {short title}
**Status:** open
**Found:** YYYY-MM-DD ({session/commit context})
**Location:** {path/to/file.ext:line, or "cross-cutting"}
**Severity:** {low | medium | high}
**Description.** {What's wrong and why it matters.}
**Reproduction.** {Minimum steps to trigger.}
**Notes.** {Related context, suggested fix, links to BUG/IMP/D/AV entries.}
```
