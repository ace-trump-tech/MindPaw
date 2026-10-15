#!/usr/bin/env python3
"""Capture a bounded JPEG or PGM over UART; pip install pyserial first."""
import argparse
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--gray", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    with serial.Serial(args.port, 115200, timeout=1) as connection:
        # Opening a USB UART can reset the board. Allow startup before sending.
        time.sleep(8)
        connection.reset_input_buffer()
        connection.write(b"camera gray\n" if args.gray else b"camera jpeg\n")
        deadline = time.monotonic() + 30
        prefix = b"CAMERA_GRAY " if args.gray else b"CAMERA_JPEG "
        while time.monotonic() < deadline:
            line = connection.readline().strip()
            if line.startswith(b"CAMERA_ERROR"):
                raise SystemExit(line.decode("ascii", errors="replace"))
            if not line.startswith(prefix):
                continue
            length = int(line[len(prefix):])
            if not 0 < length <= 20000 or (args.gray and length != 1200):
                raise SystemExit("Invalid frame length")
            data = bytearray()
            while len(data) < length and time.monotonic() < deadline:
                data.extend(connection.read(length - len(data)))
            if len(data) != length:
                raise SystemExit("Incomplete frame")
            if args.gray:
                data = b"P5\n40 30\n255\n" + data
            elif not (data.startswith(b"\xff\xd8") and data.endswith(b"\xff\xd9")):
                raise SystemExit("Invalid JPEG markers")
            args.output.write_bytes(data)
            print(f"Saved {args.output}; open it to verify the actual scene.")
            return
        raise SystemExit("Camera response timed out")


if __name__ == "__main__":
    main()
