#!/usr/bin/env python3
import argparse
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description="Read ESP8266 serial output.")
    parser.add_argument("--port", default="/dev/cu.usbserial-110")
    parser.add_argument("--baud", type=int, default=74880)
    parser.add_argument("--seconds", type=float, default=8.0)
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    try:
        # Keep GPIO0 released, then pulse reset via RTS on common ESP8266 USB boards.
        ser.dtr = False
        ser.rts = True
        time.sleep(0.2)
        ser.rts = False

        end = time.monotonic() + args.seconds
        chunks: list[bytes] = []
        while time.monotonic() < end:
            data = ser.read(4096)
            if data:
                chunks.append(data)

        raw = b"".join(chunks)
        if not raw:
            print("No boot log captured.")
            return 2

        sys.stdout.buffer.write(raw)
        if not raw.endswith(b"\n"):
            sys.stdout.buffer.write(b"\n")
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
