#!/usr/bin/env python3
"""can_monitor.py — read-only trace of the project's frames on vcan0.

AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 15.6 and 15.8. This is the
third observation surface: proof that a value really crossed the network,
independent of what either ECU claims in its log.

REQ-HMI-001: standard library only — `socket.AF_CAN` with `CAN_RAW`
directly, no `python-can`.
REQ-HMI-004: read-only. This module never writes to the bus, and the
decoding below is derived from the spec's payload tables, not from ECU
source (REQ-ARCH-011).

Example:
    python3 tools/hmi/can_monitor.py --can-if vcan0
"""
# See hw_link.py for why annotations are made lazy.
from __future__ import annotations

import argparse
import socket
import struct
import sys
import time

# struct can_frame: can_id (u32), can_dlc (u8), 3 pad bytes, 8 data bytes.
CAN_FRAME_FORMAT = "=IB3x8s"
CAN_FRAME_SIZE = struct.calcsize(CAN_FRAME_FORMAT)
CAN_SFF_MASK = 0x000007FF

VEHICLE_STATUS_ID = 0x180
CLUSTER_STATUS_ID = 0x181

CLUSTER_STATE_NAMES = {0: "OFF", 1: "NORMAL", 2: "OVERSPEED", 3: "COMM_FAULT"}


def decode_vehicle_status(data: bytes) -> str:
    """Section 6.3 payload layout."""
    speed_raw = data[0] | (data[1] << 8)
    ignition = data[2] & 0x01
    counter = data[3] & 0x0F
    crc = data[4]
    return (f"VehicleStatus speed={speed_raw / 100.0:6.2f} km/h ignition={'on' if ignition else 'off'} "
            f"counter={counter:2d} crc=0x{crc:02X}")


def decode_cluster_status(data: bytes) -> str:
    """Section 6.4 payload layout."""
    state = data[0]
    speed_raw = data[1] | (data[2] << 8)
    flags = data[3] & 0x07
    counter = data[4] & 0x0F
    crc = data[5]
    lamps = []
    if flags & 0x01:
        lamps.append("OVERSPEED")
    if flags & 0x02:
        lamps.append("TIMEOUT")
    if flags & 0x04:
        lamps.append("INTEGRITY")
    return (f"ClusterStatus state={CLUSTER_STATE_NAMES.get(state, state):10s} "
            f"speed={speed_raw / 100.0:6.2f} km/h flags=0x{flags:02X}[{','.join(lamps) or '-'}] "
            f"counter={counter:2d} crc=0x{crc:02X}")


def decode(can_id: int, data: bytes) -> str:
    if can_id == VEHICLE_STATUS_ID:
        return decode_vehicle_status(data)
    if can_id == CLUSTER_STATUS_ID:
        return decode_cluster_status(data)
    return "(not a frame this project defines)"


def open_can_socket(interface: str) -> socket.socket:
    sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    sock.bind((interface,))
    return sock


def format_line(can_id: int, dlc: int, data: bytes, monotonic_start: float) -> str:
    elapsed_ms = int((time.monotonic() - monotonic_start) * 1000.0)
    hex_payload = " ".join(f"{byte:02X}" for byte in data[:dlc])
    return f"[{elapsed_ms:6d} ms] 0x{can_id:03X} [{dlc}] {hex_payload:<23s} {decode(can_id, data)}"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--can-if", default="vcan0", help="SocketCAN interface to observe")
    parser.add_argument("--count", type=int, default=0, help="stop after this many frames; 0 means run until Ctrl+C")
    args = parser.parse_args(argv)

    try:
        sock = open_can_socket(args.can_if)
    except OSError as error:
        print(f"cannot open {args.can_if}: {error}", file=sys.stderr)
        print("hint: run tools/setup_vcan.sh first", file=sys.stderr)
        return 1

    print(f"tracing {args.can_if} (read-only), Ctrl+C to stop")
    started = time.monotonic()
    seen = 0
    try:
        while True:
            frame = sock.recv(CAN_FRAME_SIZE)
            if len(frame) < CAN_FRAME_SIZE:
                continue
            raw_id, dlc, data = struct.unpack(CAN_FRAME_FORMAT, frame)
            print(format_line(raw_id & CAN_SFF_MASK, dlc, data, started))
            seen += 1
            if args.count and seen >= args.count:
                break
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
