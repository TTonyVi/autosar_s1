#!/usr/bin/env python3
"""IT-01 Normal end-to-end communication (spec section 14.2).

1. Create and bring up vcan0 (assumed already up; see tests/integration/README).
2. Start cluster_ecu, sensor_ecu, and scenario_player.py with scenarios/default.csv.
3. Observe at least 3 seconds.
4. Assert CAN IDs 0x180 and 0x181 are present at expected approximate rates.
5. Assert cluster reaches OFF, NORMAL, and OVERSPEED according to the scenario.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from it_common import (  # noqa: E402
    CLUSTER_ECU, CLUSTER_STATUS_ID, REPO_ROOT, SENSOR_ECU, VEHICLE_STATUS_ID,
    CanObserver, Ecu, cleanup_link_files, decode_cluster_status, decode_vehicle_status,
    require_binaries, require_vcan, run_case,
)
from it_common import ItFailure  # noqa: E402

SENSOR_LINK = f"/tmp/it01_sensor_{os.getpid()}.sock"
DISPLAY_LINK = f"/tmp/it01_display_{os.getpid()}.sock"
OBSERVE_S = 4.0


def test_rates_and_state_sequence() -> None:
    cleanup_link_files(SENSOR_LINK, DISPLAY_LINK)
    observer = CanObserver()
    try:
        with Ecu(CLUSTER_ECU, ["--display-link", DISPLAY_LINK], "cluster") as cluster, \
             Ecu(SENSOR_ECU, ["--sensor-link", SENSOR_LINK], "sensor") as sensor:
            time.sleep(0.3)
            player = subprocess.Popen(
                [sys.executable, os.path.join(REPO_ROOT, "tools/hmi/scenario_player.py"),
                 "--scenario", os.path.join(REPO_ROOT, "scenarios/default.csv"),
                 "--link", SENSOR_LINK, "--speed-factor", "1.0"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            try:
                frames = observer.drain(OBSERVE_S)
            finally:
                player.terminate()
                player.wait(timeout=2)

            if not cluster.is_alive() or not sensor.is_alive():
                raise ItFailure(
                    f"an ECU exited during the test "
                    f"(cluster alive={cluster.is_alive()}, sensor alive={sensor.is_alive()})\n"
                    f"--- sensor log ---\n{sensor.read_log()}\n"
                    f"--- cluster log ---\n{cluster.read_log()}"
                )

            vehicle_frames = [f for f in frames if f[0] == VEHICLE_STATUS_ID]
            cluster_frames = [f for f in frames if f[0] == CLUSTER_STATUS_ID]

            # section 6.2: 20 ms period -> ~50/s, wide tolerance for a
            # possibly loaded host (REQ-OS-003's own qualifier).
            expected_vehicle = OBSERVE_S * 1000.0 / 20.0
            if not (0.5 * expected_vehicle <= len(vehicle_frames) <= 1.5 * expected_vehicle):
                raise ItFailure(
                    f"0x180 rate off: saw {len(vehicle_frames)} frames in {OBSERVE_S}s, "
                    f"expected ~{expected_vehicle:.0f}"
                )

            # section 6.2: 100 ms period -> ~10/s.
            expected_cluster = OBSERVE_S * 1000.0 / 100.0
            if not (0.5 * expected_cluster <= len(cluster_frames) <= 1.5 * expected_cluster):
                raise ItFailure(
                    f"0x181 rate off: saw {len(cluster_frames)} frames in {OBSERVE_S}s, "
                    f"expected ~{expected_cluster:.0f}"
                )

            states_seen = {decode_cluster_status(f[1])["state"] for f in cluster_frames}
            # scenarios/default.csv: ignition off, then on at low speed, then
            # 105 km/h (above the 100.00 km/h entry threshold) -> OFF,
            # NORMAL, OVERSPEED must all appear.
            for expected_state, name in ((0, "OFF"), (1, "NORMAL"), (2, "OVERSPEED")):
                if expected_state not in states_seen:
                    raise ItFailure(
                        f"cluster never reached {name} (states seen: {sorted(states_seen)})"
                    )

            # Sanity: the vehicle status alive counter actually advances,
            # proving these are live, distinct frames, not one cached copy.
            counters = [decode_vehicle_status(f[1])["counter"] for f in vehicle_frames]
            if len(set(counters)) < 2:
                raise ItFailure("0x180 alive counter never changed across the observation window")
    finally:
        observer.close()
        cleanup_link_files(SENSOR_LINK, DISPLAY_LINK)


def main() -> int:
    require_binaries()
    require_vcan()
    ok = run_case("IT-01 normal end-to-end communication", test_rates_and_state_sequence)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
