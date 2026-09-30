#!/usr/bin/env python3
"""IT-03 Integrity rejection (spec section 14.2).

1. Stop sensor_ecu (isolating its Tx so nothing else contends for 0x180).
2. Inject a 0x180 frame with invalid CRC.
3. Assert application value is unchanged and the integrity DEM event fails.
4. Inject valid consecutive frames.
5. Assert the event passes and application data updates.

Frames are constructed by hand with `it_common.build_vehicle_status_frame`
and sent directly onto vcan0 with `RawCanSender` -- standing in for a
malfunctioning or hostile node, which no correctly-functioning ECU in
this project would ever produce on its own. The independent Python CRC8
implementation is verified against all three of the spec's own section
6.5 golden vectors before being trusted here (see it_common.py).

A note on "application value is unchanged": this project's already-tested
Phase 5 behavior (test_cluster_swc's TestCommunicationInvalidityOutranksEverything)
has ClusterControlSwc treat ANY single invalid reception -- not just a
timeout -- as forcing CLUSTER_STATE_COMM_FAULT with DisplayedSpeedRaw
reset to 0, per section 4.4's own state diagram ("* --> COMM_FAULT:
timeout or invalid data") and REQ-ASW-001 ("communication invalidity has
higher priority than ignition and speed"). So "unchanged" is verified at
the layer REQ-ASW-005/REQ-COM-003 actually govern -- the corrupt frame's
OWN garbage value must never leak into ClusterStatus -- rather than
demanding the cluster's displayed speed freeze at its pre-fault value
across a state transition section 4.4 explicitly mandates.
"""
from __future__ import annotations

import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from it_common import (  # noqa: E402
    CLUSTER_ECU, CLUSTER_STATUS_ID, RawCanSender, VEHICLE_STATUS_ID, CanObserver, Ecu,
    build_vehicle_status_frame, cleanup_link_files, decode_cluster_status, require_binaries,
    require_vcan, run_case,
)
from it_common import ItFailure  # noqa: E402

DISPLAY_LINK = f"/tmp/it03_display_{os.getpid()}.sock"

# DEM_EVENT_VEHICLE_STATUS_INTEGRITY == 2 (bsw/services/Dem/Dem.h).
INTEGRITY_FAILED_RE = re.compile(r"event 2 failed")
INTEGRITY_PASSED_RE = re.compile(r"event 2 passed")


def test_bad_crc_frame_is_rejected_then_recovers() -> None:
    cleanup_link_files(DISPLAY_LINK)
    observer = CanObserver()
    raw = RawCanSender()
    try:
        # No sensor_ecu at all: ClusterEcu's only view of 0x180 is what
        # this test injects by hand.
        with Ecu(CLUSTER_ECU, ["--display-link", DISPLAY_LINK], "cluster") as cluster:
            time.sleep(0.3)

            # 1. Establish normal communication with hand-built, VALID
            # frames (three, for REQ-ASW-004's recovery rule) so there is
            # an established application value to protect.
            established_speed = 4000
            for counter in range(3):
                raw.send(VEHICLE_STATUS_ID,
                         build_vehicle_status_frame(established_speed, True, counter))
                time.sleep(0.03)

            normal = observer.wait_for(
                CLUSTER_STATUS_ID, lambda data: decode_cluster_status(data)["state"] != 3, 2.0
            )
            if normal is None:
                raise ItFailure("cluster never left COMM_FAULT after 3 valid hand-built frames")

            # 2. Inject a frame with a deliberately wrong CRC, a
            # different speed, and the correct next counter (so ONLY the
            # CRC is wrong -- isolating exactly what is under test).
            bad_speed = 9999
            raw.send(VEHICLE_STATUS_ID,
                     build_vehicle_status_frame(bad_speed, True, 3, corrupt_crc=True))
            time.sleep(0.3)

            # 3. The corrupt frame's OWN value must never leak into
            # ClusterStatus (REQ-COM-003/REQ-ASW-005); the integrity
            # event must have failed. The cluster correctly transitions
            # to COMM_FAULT here (section 4.4's own diagram: "* -->
            # COMM_FAULT: timeout or invalid data") -- that is a
            # different, already-tested guarantee (REQ-ASW-001), not a
            # violation of this one.
            frame = observer.wait_for(CLUSTER_STATUS_ID, lambda _d: True, 1.0)
            if frame is None:
                raise ItFailure("no ClusterStatus frame observed after the bad-CRC injection")
            decoded = decode_cluster_status(frame)
            if decoded["speed_raw"] == bad_speed:
                raise ItFailure(
                    f"the corrupt frame's speed ({bad_speed}) leaked into ClusterStatus "
                    f"-- REQ-COM-003/REQ-ASW-005 violated"
                )
            if decoded["state"] != 3:  # COMM_FAULT, per section 4.4
                raise ItFailure(
                    f"expected COMM_FAULT after a single invalid frame (section 4.4), "
                    f"got state={decoded['state']}"
                )
            if decoded["speed_raw"] != 0:
                raise ItFailure(
                    f"COMM_FAULT must display 0 (test_cluster_swc's own contract), "
                    f"got speed_raw={decoded['speed_raw']}"
                )

            log = cluster.read_log()
            if not INTEGRITY_FAILED_RE.search(log):
                raise ItFailure(f"DEM_EVENT_VEHICLE_STATUS_INTEGRITY never failed\n{log[-1000:]}")

            # 4. Inject a run of valid frames again (a plausible receiver
            # implementation might need more than one to fully recover
            # its "last valid" bookkeeping after a rejected counter).
            new_speed = 6000
            for i, counter in enumerate((3, 4, 5)):
                raw.send(VEHICLE_STATUS_ID, build_vehicle_status_frame(new_speed, True, counter))
                time.sleep(0.03)

            # 5. Assert the event passes again and the new value is
            # actually adopted.
            updated = observer.wait_for(
                CLUSTER_STATUS_ID, lambda data: decode_cluster_status(data)["speed_raw"] == new_speed,
                2.0,
            )
            if updated is None:
                raise ItFailure(f"ClusterStatus never adopted the new value ({new_speed}) after recovery")

            time.sleep(0.3)
            log = cluster.read_log()
            if not INTEGRITY_PASSED_RE.search(log):
                raise ItFailure(f"DEM_EVENT_VEHICLE_STATUS_INTEGRITY never passed again\n{log[-1000:]}")
    finally:
        raw.close()
        observer.close()
        cleanup_link_files(DISPLAY_LINK)


def main() -> int:
    require_binaries()
    require_vcan()
    ok = run_case("IT-03 integrity rejection", test_bad_crc_frame_is_rejected_then_recovers)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
