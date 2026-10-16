#!/usr/bin/env python3
"""Copy the board's health log off the TF card over USB, without removing the card.

Usage:  ~/.platformio/penv/bin/python tools/sdlog.py [-p /dev/ttyACM0] [-o health.csv]
        (needs pyserial; PlatformIO's Python has it)

Sends `dump` to the running firmware, which replies
    BEGIN <path> <size>\\n<file bytes>\\nEND <bytes sent>\\n
and saves the bytes. Fails if the byte count doesn't match. Opening the port
does not reset the board. Then:  python3 tools/soak.py health.csv --since ...
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing: run with PlatformIO's Python (pipx: ~/.local/share/pipx/venvs/platformio/bin/python)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    ap.add_argument("-o", "--out", default="health.csv")
    ap.add_argument("--timeout", type=float, default=30.0, help="seconds to wait for the whole transfer")
    args = ap.parse_args()

    # Open with pyserial's default line states. Forcing DTR/RTS low before
    # opening pulses the ESP32-S3's USB-Serial/JTAG reset (seen 2026-10-16).
    s = serial.Serial(args.port, 115200, timeout=0.5)
    s.reset_input_buffer()
    s.write(b"dump\n")

    deadline = time.time() + args.timeout
    size = None
    while time.time() < deadline:  # skip ordinary log lines until BEGIN
        line = s.readline()
        if line.startswith(b"BEGIN "):
            size = int(line.split()[-1])
            break
    if size is None:
        sys.exit("no BEGIN from the board (is the Boreas firmware running?)")
    if size < 0:
        sys.exit("board reports no log file on the card")

    data = bytearray()
    while len(data) < size and time.time() < deadline:
        data += s.read(size - len(data))
    tail = b""
    while time.time() < deadline and b"END " not in tail:
        tail += s.readline()
    s.close()

    sent = int(tail.split(b"END ")[-1].split()[0]) if b"END " in tail else -1
    if len(data) != size or sent != size:
        sys.exit(f"incomplete transfer: expected {size}, received {len(data)}, board sent {sent}")
    with open(args.out, "wb") as f:
        f.write(data)
    print(f"saved {len(data)} bytes ({data.count(10)} lines) to {args.out}")


if __name__ == "__main__":
    main()
