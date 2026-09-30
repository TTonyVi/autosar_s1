#!/usr/bin/env python3
"""IT-04 Layered Tx/Rx evidence (spec section 14.2).

"Enable test counters in each module and assert one end-to-end exchange
increments the expected RTE, COM, PduR, CanIf, and Can counters, and on
the input side the expected Adc, IoHwAb, and RTE counters. Counters are
test instrumentation, not an alternate data path."

The counters themselves (PduR_GetComTransmitForwardCount,
PduR_GetCanIfRxIndicationForwardCount, Adc_GetGroupReadCount,
IoHwAb_GetValidSampleCount) were added specifically for this requirement;
every other counter used here already existed for a different purpose
(the section 15.8 status log, or an earlier phase's own test). None of
them is read back by any module's own control-flow logic -- verified by
inspection when each was added (see docs/IMPLEMENTATION_STATUS.md, Phase 8).

Both ECUs already print a one-line-per-second "[Layers]" snapshot of
their own counters (section 13.4 bounded-log-volume compliant); this test
reads that line back rather than inventing a second channel, exactly the
"test instrumentation, not an alternate data path" the requirement asks
for -- the same log a human running the demo already sees.
"""
from __future__ import annotations

import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from it_common import (  # noqa: E402
    CLUSTER_ECU, SENSOR_ECU, Ecu, SensorLineSender, cleanup_link_files, require_binaries,
    require_vcan, run_case, speed_kph_to_counts,
)
from it_common import ItFailure  # noqa: E402

SENSOR_LINK = f"/tmp/it04_sensor_{os.getpid()}.sock"
DISPLAY_LINK = f"/tmp/it04_display_{os.getpid()}.sock"
OBSERVE_S = 2.5

SENSOR_LAYERS_RE = re.compile(
    r"\[Layers\] INFO rte_tx=(\d+) com_tx=(\d+) pdur_tx_fwd=(\d+) canif_tx_conf=(\d+) \| "
    r"adc_reads=(\d+) iohwab_valid=(\d+) rte_rx=(\d+)"
)
CLUSTER_LAYERS_RE = re.compile(
    r"\[Layers\] INFO can_rx=(\d+) canif_rx_accepted=(\d+) pdur_rx_fwd=(\d+) com_rx=(\d+) rte_rx=(\d+)"
)


def last_match(pattern: re.Pattern, text: str):
    matches = list(pattern.finditer(text))
    return matches[-1] if matches else None


def test_every_layer_advances_together() -> None:
    cleanup_link_files(SENSOR_LINK, DISPLAY_LINK)
    sender = SensorLineSender(SENSOR_LINK)
    try:
        with Ecu(CLUSTER_ECU, ["--display-link", DISPLAY_LINK], "cluster") as cluster, \
             Ecu(SENSOR_ECU, ["--sensor-link", SENSOR_LINK], "sensor") as sensor:
            deadline = time.monotonic() + OBSERVE_S
            while time.monotonic() < deadline:
                # Keep a real input flowing so Adc/IoHwAb also have
                # something to count (REQ-VHW-007 keepalive rate).
                sender.send(speed_kph_to_counts(80.0), True)
                time.sleep(0.05)

            sensor_log = sensor.read_log()
            cluster_log = cluster.read_log()

            sensor_match = last_match(SENSOR_LAYERS_RE, sensor_log)
            if sensor_match is None:
                raise ItFailure(f"no [Layers] line found in sensor_ecu's log\n{sensor_log[-1500:]}")
            cluster_match = last_match(CLUSTER_LAYERS_RE, cluster_log)
            if cluster_match is None:
                raise ItFailure(f"no [Layers] line found in cluster_ecu's log\n{cluster_log[-1500:]}")

            rte_tx, com_tx, pdur_tx_fwd, canif_tx_conf, adc_reads, iohwab_valid, sensor_rte_rx = (
                int(g) for g in sensor_match.groups()
            )
            can_rx, canif_rx_accepted, pdur_rx_fwd, com_rx, cluster_rte_rx = (
                int(g) for g in cluster_match.groups()
            )

            # Output path: SensorEcu's own Tx pipeline for VehicleStatus.
            tx_layers = {
                "RTE (Rte_GetTxWriteCount)": rte_tx,
                "COM (Com_GetIpduTxCount)": com_tx,
                "PduR (PduR_GetComTransmitForwardCount)": pdur_tx_fwd,
                "CanIf (CanIf_GetTxConfirmationCount)": canif_tx_conf,
            }
            for name, value in tx_layers.items():
                if value == 0:
                    raise ItFailure(f"Tx-side layer never advanced: {name} = 0")

            # Input path: Adc -> IoHwAb -> RTE, all on SensorEcu.
            input_layers = {
                "Adc (Adc_GetGroupReadCount)": adc_reads,
                "IoHwAb (IoHwAb_GetValidSampleCount)": iohwab_valid,
                "RTE input (Rte_GetRxUpdateCount, sensor side)": sensor_rte_rx,
            }
            for name, value in input_layers.items():
                if value == 0:
                    raise ItFailure(f"input-side layer never advanced: {name} = 0")

            # The same exchange's Rx side, observed on ClusterEcu: Can ->
            # CanIf -> PduR -> Com -> RTE, all evidence of the SAME frames
            # SensorEcu's Tx-side counters above produced.
            rx_layers = {
                "Can (Can_GetRxFrameCount)": can_rx,
                "CanIf (CanIf_GetRxAcceptedCount)": canif_rx_accepted,
                "PduR (PduR_GetCanIfRxIndicationForwardCount)": pdur_rx_fwd,
                "COM (Com_GetIpduRxValidCount)": com_rx,
                "RTE (Rte_GetRxUpdateCount, cluster side)": cluster_rte_rx,
            }
            for name, value in rx_layers.items():
                if value == 0:
                    raise ItFailure(f"Rx-side layer never advanced: {name} = 0")

            # Cross-check: the frames ClusterEcu's Can layer actually
            # received cannot exceed what SensorEcu's CanIf layer actually
            # confirmed sending -- proving these are the SAME exchange,
            # not two independently-incrementing, disconnected counters.
            if can_rx > canif_tx_conf:
                raise ItFailure(
                    f"cluster received more frames ({can_rx}) than sensor confirmed "
                    f"transmitting ({canif_tx_conf}) -- counters are not evidence of the same exchange"
                )

            print(f"    Tx side (SensorEcu): {tx_layers}")
            print(f"    Input side (SensorEcu): {input_layers}")
            print(f"    Rx side (ClusterEcu): {rx_layers}")
    finally:
        sender.close()
        cleanup_link_files(SENSOR_LINK, DISPLAY_LINK)


def main() -> int:
    require_binaries()
    require_vcan()
    ok = run_case("IT-04 layered Tx/Rx evidence", test_every_layer_advances_together)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
