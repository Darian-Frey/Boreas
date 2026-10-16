#!/usr/bin/env python3
"""Summarise a Boreas health log (SPEC.md §Health log) for the Phase 1 soak.

Usage:  python3 tools/soak.py /media/<you>/<card>/boreas/health.csv [--since "YYYY-MM-DD HH:MM"]

Reports boots with their reset reasons (times estimated for boots before the
clock was trusted), then per boot session: length, free-heap trend (AV-002),
minimum free heap, smallest largest-free-block, fetch success, Wi-Fi
reconnects, NTP syncs; and fetch failures by reason and by minute past the
hour. Standard library only. Exit 0 PASS; 1 FAIL (unexpected reset, or free
heap falling faster than --max-heap-drop B/day); 2 INCOMPLETE (healthy, but
no uninterrupted run reached --min-hours, default 72).
"""

import argparse
import csv
import statistics
import sys
from datetime import datetime

EXPECTED_RESETS = {"POWERON", "USB", "SW", "EXT"}  # power-up, flashing, deliberate restarts


def load(path, since):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            try:
                for k in ("uptime_s", "free_heap", "min_free_heap", "max_block", "fetch_ok", "fetch_fail",
                          "wifi_reconnects", "ntp_syncs"):
                    r[k] = int(r[k])
                r["utc"] = int(r["utc"])
            except (KeyError, ValueError, TypeError):
                continue  # torn line from a power cut, or a header repeated
            rows.append(r)

    # Rows logged before the clock was trusted have utc 0. Estimate their time
    # from the next timed row of the same boot: its utc minus the uptime between.
    for i, r in enumerate(rows):
        r["est"] = r["utc"] or None
        if r["est"] is None:
            for n in rows[i + 1:]:
                if n["event"] == "boot":
                    break
                if n["utc"] > 0 and n["uptime_s"] >= r["uptime_s"]:
                    r["est"] = n["utc"] - (n["uptime_s"] - r["uptime_s"])
                    break
        r["when"] = datetime.fromtimestamp(r["est"]).strftime("%Y-%m-%d %H:%M") if r["est"] else "?"

    if since:
        rows = [r for r in rows if r["when"] == "?" or r["when"] >= since]
    return rows


def sessions(rows):
    """Split rows into boot sessions (a boot row starts a new one)."""
    out, cur = [], []
    for r in rows:
        if r["event"] == "boot" and cur:
            out.append(cur)
            cur = []
        cur.append(r)
    if cur:
        out.append(cur)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--since", help='ignore rows before this local time, e.g. "2026-10-12 09:00"')
    ap.add_argument("--max-heap-drop", type=float, default=2048, help="allowed free-heap fall, bytes/day")
    ap.add_argument("--min-hours", type=float, default=72, help="required longest uninterrupted run")
    args = ap.parse_args()

    rows = load(args.csv, args.since)
    if not rows:
        print("no usable rows")
        return 1

    timed = sorted(r["est"] for r in rows if r["est"])
    span_h = (timed[-1] - timed[0]) / 3600 if len(timed) > 1 else 0.0
    print(f"rows {len(rows)}  span {span_h:.1f} h  ({rows[0]['when']} -> {rows[-1]['when']})")

    boots = [r for r in rows if r["event"] == "boot"]
    unexpected = [b for b in boots if b["note"] not in EXPECTED_RESETS]
    print(f"boots {len(boots)}: " + (", ".join(f"{b['when']} {b['note']}" for b in boots) or "none"))
    if unexpected:
        print(f"  !! unexpected resets: " + ", ".join(f"{b['when']} {b['note']}" for b in unexpected))

    # Per boot session: duration, heap trend, fetches, reconnects.
    print("runs:")
    worst_slope, longest_h = 0.0, 0.0
    for sess in sessions(rows):
        ticks = [r for r in sess if r["event"] == "tick"]
        if not ticks:
            continue
        hours = ticks[-1]["uptime_s"] / 3600
        longest_h = max(longest_h, hours)
        last = ticks[-1]
        total = last["fetch_ok"] + last["fetch_fail"]
        rate = 100.0 * last["fetch_ok"] / total if total else 0.0
        line = f"  {sess[0]['when']}  {hours:5.1f} h  {len(ticks):3d} ticks"
        if len(ticks) >= 3:
            x = [r["uptime_s"] / 86400 for r in ticks]
            y = [r["free_heap"] for r in ticks]
            slope = statistics.linear_regression(x, y).slope
            worst_slope = min(worst_slope, slope)
            line += (f"  heap {y[0]}->{y[-1]} ({slope:+.0f} B/day)  min-ever {min(r['min_free_heap'] for r in ticks)}"
                     f"  block {min(r['max_block'] for r in ticks)}")
        line += (f"  fetch {last['fetch_ok']}/{total} ({rate:.0f} %)  reconnects {last['wifi_reconnects']}"
                 f"  ntp {last['ntp_syncs']}")
        print(line)

    fails = [r for r in rows if r["event"] == "fetch_fail"]
    if fails:
        reasons = {}
        for r in fails:
            reasons[r["note"]] = reasons.get(r["note"], 0) + 1
        print("fetch failures: " + "; ".join(f"{k} x{v}" for k, v in sorted(reasons.items(), key=lambda kv: -kv[1])))
        minutes = {}
        for r in fails:
            if r["est"]:
                m = datetime.fromtimestamp(r["est"]).strftime(":%M")
                minutes[m] = minutes.get(m, 0) + 1
        print("  by minute past the hour: " + ", ".join(f"{k} x{v}" for k, v in sorted(minutes.items())))

    healthy = not unexpected and worst_slope > -args.max_heap_drop
    long_enough = longest_h >= args.min_hours
    print(f"longest uninterrupted run {longest_h:.1f} h (target {args.min_hours:.0f} h)")
    if not healthy:
        print("RESULT: FAIL")
        return 1
    if not long_enough:
        print("RESULT: INCOMPLETE (healthy so far, but no run reached the target)")
        return 2
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
