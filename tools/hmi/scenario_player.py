#!/usr/bin/env python3
"""scenario_player.py — replays a deterministic scenario onto the sensor line.

AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 15.7.

It exists so that integration tests exercise exactly the path a human demo
exercises: there is no second input mechanism inside the ECU. The ECU sees
the same ADC counts and ignition levels whether a slider or this file
produced them.

Standard library only (REQ-HMI-001). This is tooling (spec section 12.1):
never imported by, or linked into, an ECU target.

Examples:
    python3 tools/hmi/scenario_player.py --scenario scenarios/default.csv \
        --link /tmp/vhw_sensor.sock
    python3 tools/hmi/scenario_player.py --scenario scenarios/default.csv --loop \
        --speed-factor 4
"""
# PEP 585 generics (list[str]) are written throughout because REQ-HMI-001
# specifies Python 3.10 or newer. This import makes annotations lazy so the
# same source also runs unchanged on the 3.8 interpreters still shipped by
# Ubuntu 20.04, where evaluating those annotations at def time would raise.
from __future__ import annotations

import argparse
import csv
import itertools
import socket
import sys
import time

EXPECTED_HEADER = ["time_ms", "vehicle_speed_kph", "ignition_on"]

# REQ-HMI-003: counts = round(kph * 100 * 4095 / 25000), the inverse of the
# IoHwAb scaling. Shown to the reader rather than hidden.
ADC_FULL_SCALE_COUNTS = 4095
SPEED_RAW_FULL_SCALE = 25000  # 250.00 km/h in 0.01 km/h units
KEEPALIVE_PERIOD_S = 0.100  # REQ-VHW-007


class ScenarioError(Exception):
    """A malformed scenario. Carries the offending line number."""

    def __init__(self, line_number: int, message: str) -> None:
        super().__init__(f"line {line_number}: {message}")
        self.line_number = line_number


def speed_kph_to_counts(speed_kph: float) -> int:
    speed_raw = round(speed_kph * 100.0)
    counts = round(speed_raw * ADC_FULL_SCALE_COUNTS / SPEED_RAW_FULL_SCALE)
    return max(0, min(ADC_FULL_SCALE_COUNTS, counts))


def parse_scenario(path: str) -> list[tuple[int, float, bool]]:
    """Parses and fully validates the CSV before a single byte is sent.

    Section 15.7: a malformed scenario SHALL NOT send partial data, so the
    whole file is validated up front rather than row by row while playing.
    """
    rows: list[tuple[int, float, bool]] = []
    with open(path, newline="", encoding="ascii") as handle:
        reader = csv.reader(handle)
        try:
            header = next(reader)
        except StopIteration:
            raise ScenarioError(1, "file is empty; expected the header row") from None
        if [field.strip() for field in header] != EXPECTED_HEADER:
            raise ScenarioError(1, f"header must be exactly {','.join(EXPECTED_HEADER)}")

        previous_time = None
        for line_number, row in enumerate(reader, start=2):
            if not row or all(field.strip() == "" for field in row):
                continue  # tolerate a trailing newline, nothing else
            if len(row) != 3:
                raise ScenarioError(line_number, f"expected 3 fields, found {len(row)}")

            raw_time, raw_speed, raw_ignition = (field.strip() for field in row)

            if not raw_time.isdigit():
                raise ScenarioError(line_number, f"time_ms must be an unsigned integer, got {raw_time!r}")
            time_ms = int(raw_time)
            if previous_time is None:
                if time_ms != 0:
                    raise ScenarioError(line_number, "the first time_ms must be 0")
            elif time_ms <= previous_time:
                raise ScenarioError(line_number, f"time_ms must strictly increase, got {time_ms} after {previous_time}")
            previous_time = time_ms

            if "." in raw_speed and len(raw_speed.split(".", 1)[1]) > 2:
                raise ScenarioError(line_number, f"vehicle_speed_kph allows at most two decimals, got {raw_speed!r}")
            try:
                speed_kph = float(raw_speed)
            except ValueError:
                raise ScenarioError(line_number, f"vehicle_speed_kph is not a number: {raw_speed!r}") from None
            if not 0.0 <= speed_kph <= 250.0:
                raise ScenarioError(line_number, f"vehicle_speed_kph must be 0.00..250.00, got {speed_kph}")

            if raw_ignition not in ("0", "1"):
                raise ScenarioError(line_number, f"ignition_on must be 0 or 1, got {raw_ignition!r}")

            rows.append((time_ms, speed_kph, raw_ignition == "1"))

    if not rows:
        raise ScenarioError(2, "scenario contains no rows")
    return rows


def play(rows, link_path: str, speed_factor: float, loop: bool, verbose: bool) -> None:
    """Sends step changes at their scheduled times, with a 100 ms keepalive.

    REQ-VHW-007: the producer repeats its current value at least every
    100 ms even when unchanged, so that absence is detectable by the ECU.
    """
    sequence = itertools.count(1)
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock.setblocking(False)

    try:
        while True:
            started = time.monotonic()
            index = 0
            current = rows[0]
            end_ms = rows[-1][0]

            while True:
                elapsed_ms = (time.monotonic() - started) * 1000.0 * speed_factor
                while (index + 1) < len(rows) and rows[index + 1][0] <= elapsed_ms:
                    index += 1
                    current = rows[index]
                    if verbose:
                        print(f"[{int(elapsed_ms):6d} ms] step -> {current[1]:.2f} km/h, ignition {'on' if current[2] else 'off'}")

                counts = speed_kph_to_counts(current[1])
                message = f"ADC0={counts};DIO0={1 if current[2] else 0};SEQ={next(sequence)}"
                try:
                    sock.sendto(message.encode("ascii"), link_path)
                except (FileNotFoundError, ConnectionRefusedError, BlockingIOError, OSError):
                    # REQ-HMI-005/REQ-VHW-005: an absent peer is a normal
                    # condition on a wire, never a reason to stop.
                    pass

                if elapsed_ms >= end_ms and index == len(rows) - 1 and not loop:
                    # The final row stays active until the player exits;
                    # keep the keepalive going so the ECU keeps seeing it.
                    pass
                time.sleep(KEEPALIVE_PERIOD_S / speed_factor)

                if loop and elapsed_ms >= end_ms:
                    break
    except KeyboardInterrupt:
        print("\nscenario player stopped")
    finally:
        sock.close()


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--scenario", required=True, help="path to the scenario CSV (section 15.7)")
    parser.add_argument("--link", default="/tmp/vhw_sensor.sock", help="sensor line socket path")
    parser.add_argument("--loop", action="store_true", help="repeat the scenario instead of holding the final row")
    parser.add_argument("--speed-factor", type=float, default=1.0, help="scale time; 4 replays four times faster")
    parser.add_argument("--verbose", action="store_true", help="print each step change")
    args = parser.parse_args(argv)

    if args.speed_factor <= 0.0:
        print("--speed-factor must be positive", file=sys.stderr)
        return 2

    try:
        rows = parse_scenario(args.scenario)
    except ScenarioError as error:
        # Section 15.7: a tool error, not an ECU error. Nothing was sent.
        print(f"{args.scenario}: {error}", file=sys.stderr)
        return 1
    except OSError as error:
        print(f"{args.scenario}: {error}", file=sys.stderr)
        return 1

    print(f"playing {len(rows)} steps from {args.scenario} onto {args.link}")
    play(rows, args.link, args.speed_factor, args.loop, args.verbose)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
