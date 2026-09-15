#!/usr/bin/env python3
"""hw_link.py — manual stimulation tool for a virtual hardware link.

AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 15.6.2. Sends one datagram
(or repeats one at the 100ms keepalive rate) onto a virtual hardware link,
for manual testing/debugging before the full Tkinter HMI (Phase 7) exists.

Standard library only (REQ-HMI-001) — no `pip install`. This is test/tool
instrumentation (spec section 12.1): it is never imported by, or linked
into, an ECU target.

Examples:
    python3 tools/hmi/hw_link.py --link /tmp/vhw_sensor.sock --speed 120.0 --ignition on
    python3 tools/hmi/hw_link.py --link /tmp/vhw_sensor.sock --raw "ADC0=4095;DIO0=1;SEQ=1"
"""
# PEP 585 generics (list[str]) are written throughout because REQ-HMI-001
# specifies Python 3.10 or newer. This import makes annotations lazy so the
# same source also runs unchanged on the 3.8 interpreters still shipped by
# Ubuntu 20.04, where evaluating those annotations at def time would raise.
from __future__ import annotations

import argparse
import itertools
import socket
import sys
import time

# REQ-HMI-003: counts = round(kph * 100 * 4095 / 25000), the inverse of
# IoHwAb's scaling (speed_raw = adc_counts * 25000 / 4095).
ADC_FULL_SCALE_COUNTS = 4095
SPEED_RAW_FULL_SCALE = 25000  # 250.00 km/h in 0.01 km/h units


def speed_kph_to_counts(speed_kph: float) -> int:
    speed_raw = round(speed_kph * 100.0)
    counts = round(speed_raw * ADC_FULL_SCALE_COUNTS / SPEED_RAW_FULL_SCALE)
    return max(0, min(ADC_FULL_SCALE_COUNTS, counts))


def build_sensor_message(counts: int, ignition_on: bool, seq: int) -> str:
    return f"ADC0={counts};DIO0={1 if ignition_on else 0};SEQ={seq}"


def send_once(sock: socket.socket, link_path: str, message: str) -> None:
    sock.sendto(message.encode("ascii"), link_path)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--link", required=True, help="path to the virtual hardware link's AF_UNIX socket")
    parser.add_argument("--speed", type=float, help="wheel speed in km/h (0.00..250.00); builds a sensor-line message")
    parser.add_argument("--ignition", choices=["on", "off"], default="on", help="ignition state (with --speed)")
    parser.add_argument("--raw", help="send this exact ASCII payload instead of building one from --speed")
    parser.add_argument("--repeat", action="store_true", help="keep resending every 100ms (Ctrl+C to stop)")
    args = parser.parse_args(argv)

    if args.raw is None and args.speed is None:
        parser.error("specify either --raw or --speed")
    if args.raw is not None and args.speed is not None:
        parser.error("--raw and --speed are mutually exclusive")

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    try:
        if args.raw is not None:
            send_once(sock, args.link, args.raw)
            print(f"sent: {args.raw}")
            if args.repeat:
                while True:
                    time.sleep(0.1)
                    send_once(sock, args.link, args.raw)
        else:
            counts = speed_kph_to_counts(args.speed)
            ignition_on = args.ignition == "on"
            print(f"{args.speed:.2f} km/h -> ADC0={counts} counts, ignition={args.ignition}")
            if args.repeat:
                for seq in itertools.count():
                    message = build_sensor_message(counts, ignition_on, seq)
                    send_once(sock, args.link, message)
                    time.sleep(0.1)
            else:
                message = build_sensor_message(counts, ignition_on, 0)
                send_once(sock, args.link, message)
                print(f"sent: {message}")
    except KeyboardInterrupt:
        print("\nstopped")
    except OSError as exc:
        print(f"error: could not send to '{args.link}': {exc}", file=sys.stderr)
        return 1
    finally:
        sock.close()

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
