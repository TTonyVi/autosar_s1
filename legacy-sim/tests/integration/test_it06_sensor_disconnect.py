#!/usr/bin/env python3
"""IT-06 Sensor line disconnected and restored (spec section 14.2).

1. With communication running normally, stop sending on the sensor line.
2. Assert that within 600 ms IoHwAb reports invalid, SensorEcu publishes
   speed 0 and ignition off, and DEM_EVENT_SENSOR_LINK_LOST is failed.
3. Assert ClusterEcu does NOT enter COMM_FAULT, because CAN communication
   is still healthy — this is the whole point of the test: a dead sensor
   is not a dead bus.
4. Resume sending and assert the event passes and values resume within
   100 ms.

DEM state is only observable through each ECU's own stdout log
(section 10.2 has no wire-visible DEM signal), so this test greps the
sensor_ecu log for the section 15.8 DEM transition lines rather than
reading Dem_* state directly — the same interface a human running the
demo would read.
"""
from __future__ import annotations

import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from it_common import (  # noqa: E402
    CLUSTER_ECU, CLUSTER_STATUS_ID, SENSOR_ECU, VEHICLE_STATUS_ID, CanObserver, Ecu, SensorLineSender,
    cleanup_link_files, decode_cluster_status, decode_vehicle_status, require_binaries, require_vcan,
    run_case, speed_kph_to_counts,
)
from it_common import ItFailure  # noqa: E402

SENSOR_LINK = f"/tmp/it06_sensor_{os.getpid()}.sock"
DISPLAY_LINK = f"/tmp/it06_display_{os.getpid()}.sock"

# DEM_EVENT_SENSOR_LINK_LOST == 5 (bsw/services/Dem/Dem.h); the log format
# is fixed by section 13.4 and the "event %u %s" line added in Phase 6.
LINK_LOST_FAILED_RE = re.compile(r"event 5 failed")
LINK_LOST_PASSED_RE = re.compile(r"event 5 passed")


def test_dead_sensor_is_not_a_dead_bus() -> None:
    cleanup_link_files(SENSOR_LINK, DISPLAY_LINK)
    observer = CanObserver()
    sender = SensorLineSender(SENSOR_LINK)
    try:
        # cluster is held only for its with-block lifecycle; this test
        # observes ClusterEcu purely through the bus.
        with Ecu(CLUSTER_ECU, ["--display-link", DISPLAY_LINK], "cluster"), \
             Ecu(SENSOR_ECU, ["--sensor-link", SENSOR_LINK], "sensor") as sensor:
            time.sleep(0.3)

            # 1. Normal communication: keep sending a real speed/ignition
            # for long enough to clear the initial LINK_LOST (which fires
            # at startup before any datagram has ever arrived) and reach
            # NORMAL.
            deadline = time.monotonic() + 3.0
            reached_normal = False
            while time.monotonic() < deadline and not reached_normal:
                sender.send(speed_kph_to_counts(50.0), True)
                if observer.wait_for(
                    CLUSTER_STATUS_ID, lambda data: decode_cluster_status(data)["state"] == 1, 0.15
                ):
                    reached_normal = True
            if not reached_normal:
                raise ItFailure("cluster never reached NORMAL before the disconnect step")

            # 2. Stop sending. Wait past the 500ms IoHwAb timeout + margin.
            time.sleep(0.9)

            log = sensor.read_log()
            if not LINK_LOST_FAILED_RE.search(log):
                raise ItFailure(
                    f"DEM_EVENT_SENSOR_LINK_LOST never failed within 900ms of disconnect\n"
                    f"--- sensor log tail ---\n{log[-1000:]}"
                )

            # SensorEcu must publish speed 0 / ignition off once invalid
            # (REQ-IOHWAB-003) — verified on the bus, not just in the log.
            zero_frame = observer.wait_for(
                VEHICLE_STATUS_ID,
                lambda data: decode_vehicle_status(data)["speed_raw"] == 0
                and decode_vehicle_status(data)["ignition"] is False,
                timeout_s=1.0,
            )
            if zero_frame is None:
                raise ItFailure("SensorEcu never published speed=0/ignition=off after disconnect")

            # 3. ClusterEcu must NOT enter COMM_FAULT: CAN is still healthy,
            # 0x180 keeps arriving (with speed 0), just IoHwAb-invalid.
            fault_frame = observer.wait_for(
                CLUSTER_STATUS_ID, lambda data: decode_cluster_status(data)["state"] == 3, 1.0
            )
            if fault_frame is not None:
                raise ItFailure(
                    "ClusterEcu entered COMM_FAULT from a dead SENSOR, not a dead BUS "
                    "-- this is exactly the distinction IT-06 exists to catch"
                )

            # 4. Resume sending; assert PASSED and values resume within
            # a generous margin around the spec's 100ms claim.
            sender.send(speed_kph_to_counts(50.0), True)
            resumed = observer.wait_for(
                VEHICLE_STATUS_ID, lambda data: decode_vehicle_status(data)["speed_raw"] > 0, 1.0
            )
            if resumed is None:
                raise ItFailure("speed never resumed after re-sending on the sensor line")

            time.sleep(0.3)
            log = sensor.read_log()
            if not LINK_LOST_PASSED_RE.search(log):
                raise ItFailure(f"DEM_EVENT_SENSOR_LINK_LOST never passed again\n{log[-1000:]}")
    finally:
        sender.close()
        observer.close()
        cleanup_link_files(SENSOR_LINK, DISPLAY_LINK)


def main() -> int:
    require_binaries()
    require_vcan()
    ok = run_case("IT-06 sensor line disconnected and restored", test_dead_sensor_is_not_a_dead_bus)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
