#!/usr/bin/env python3
"""Summarise a Boreas health log (SPEC.md §Health log) for the Phase 1 soak.

Usage:  python3 tools/soak.py /media/<you>/<card>/boreas/health.csv [--since "YYYY-MM-DD HH:MM"]

Reports run length, boots and their reset reasons, the free-heap trend
(AV-002), the smallest largest-free-block, fetch success rate and Wi-Fi
reconnects. Standard library only. Exit status 1 if the soak looks
unhealthy: unexpected reboots, or free heap falling by more than
--max-heap-drop bytes/day (default 2048).
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
            if since and r["local"] != "-" and r["local"] < since:
                continue
            rows.append(r)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--since", help='ignore rows before this local time, e.g. "2026-10-12 09:00"')
    ap.add_argument("--max-heap-drop", type=float, default=2048, help="allowed free-heap fall, bytes/day")
    args = ap.parse_args()

    rows = load(args.csv, args.since)
    if not rows:
        print("no usable rows")
        return 1

    boots = [r for r in rows if r["event"] == "boot"]
    ticks = [r for r in rows if r["event"] == "tick"]
    fails = [r for r in rows if r["event"] == "fetch_fail"]
    timed = sorted((r for r in rows if r["utc"] > 0), key=lambda r: r["utc"])
    span_h = (timed[-1]["utc"] - timed[0]["utc"]) / 3600 if len(timed) > 1 else 0.0

    print(f"rows {len(rows)}  ticks {len(ticks)}  span {span_h:.1f} h"
          + (f"  ({timed[0]['local']} -> {timed[-1]['local']})" if timed else ""))

    unexpected = [b for b in boots if b["note"] not in EXPECTED_RESETS]
    print(f"boots {len(boots)}: " + (", ".join(f"{b['local']} {b['note']}" for b in boots) or "none"))
    if unexpected:
        print(f"  !! unexpected resets: {len(unexpected)}")

    # Heap trend over the longest uninterrupted run of ticks (uptime increasing).
    runs, cur = [], []
    for r in ticks:
        if cur and r["uptime_s"] < cur[-1]["uptime_s"]:
            runs.append(cur)
            cur = []
        cur.append(r)
    if cur:
        runs.append(cur)
    longest = max(runs, key=len) if runs else []
    slope_per_day = 0.0
    if len(longest) >= 3:
        x = [r["uptime_s"] / 86400 for r in longest]
        y = [r["free_heap"] for r in longest]
        slope_per_day = statistics.linear_regression(x, y).slope
        print(f"free heap: first {y[0]}  last {y[-1]}  min {min(y)}  trend {slope_per_day:+.0f} B/day "
              f"over {len(longest)} ticks ({(x[-1] - x[0]) * 24:.1f} h)")
        print(f"min free heap ever {min(r['min_free_heap'] for r in longest)}  "
              f"smallest largest-block {min(r['max_block'] for r in longest)}")
    else:
        print("free heap: not enough ticks for a trend (need 3+)")

    last = rows[-1]
    total = last["fetch_ok"] + last["fetch_fail"]
    rate = 100.0 * last["fetch_ok"] / total if total else 0.0
    print(f"fetches (since last boot): ok {last['fetch_ok']}  fail {last['fetch_fail']}  ({rate:.1f} % ok)")
    if fails:
        reasons = {}
        for r in fails:
            reasons[r["note"]] = reasons.get(r["note"], 0) + 1
        print("  failure reasons: " + "; ".join(f"{k} x{v}" for k, v in sorted(reasons.items(), key=lambda kv: -kv[1])))
    print(f"wifi reconnects (since last boot): {last['wifi_reconnects']}  ntp syncs: {last['ntp_syncs']}")

    healthy = not unexpected and slope_per_day > -args.max_heap_drop
    print("RESULT:", "PASS" if healthy else "FAIL")
    return 0 if healthy else 1


if __name__ == "__main__":
    sys.exit(main())
