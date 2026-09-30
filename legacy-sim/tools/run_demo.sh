#!/usr/bin/env bash
# run_demo.sh - start the whole demonstration.
#
# AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 15.3. Starting the three
# processes by hand stays fully supported: this script is a convenience,
# never a requirement, and it starts exactly the same executables with
# exactly the same options a human would type.
#
#   bash tools/run_demo.sh                 # HMI demo (needs a display)
#   bash tools/run_demo.sh --headless      # scenario player instead of the GUI
#   bash tools/run_demo.sh --headless --duration 15
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

CAN_IF="vcan0"
SENSOR_LINK="/tmp/vhw_sensor.sock"
DISPLAY_LINK="/tmp/vhw_cluster_display.sock"
SCENARIO="scenarios/default.csv"
HEADLESS=0
DURATION=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --headless) HEADLESS=1; shift ;;
        --can-if) CAN_IF="$2"; shift 2 ;;
        --scenario) SCENARIO="$2"; shift 2 ;;
        --duration) DURATION="$2"; shift 2 ;;
        --help)
            sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

if [[ ! -x ./build/sensor_ecu || ! -x ./build/cluster_ecu ]]; then
    echo "[run_demo] ECUs are not built. Run:" >&2
    echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel" >&2
    exit 1
fi

if ! ip link show "$CAN_IF" >/dev/null 2>&1; then
    echo "[run_demo] '$CAN_IF' is missing; creating it (needs sudo)"
    bash tools/setup_vcan.sh
fi

# A virtual hardware link is a bound socket file; a leftover from a previous
# run would stop the consumer from binding (REQ-ECUM-006).
rm -f "$SENSOR_LINK" "$DISPLAY_LINK"

PIDS=()
cleanup() {
    echo
    echo "[run_demo] stopping"
    for pid in "${PIDS[@]:-}"; do
        kill "$pid" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    rm -f "$SENSOR_LINK" "$DISPLAY_LINK"
}
trap cleanup EXIT INT TERM

DURATION_ARG=()
if [[ "$DURATION" != "0" ]]; then
    DURATION_ARG=(--duration "$DURATION")
fi

echo "[run_demo] starting cluster_ecu"
./build/cluster_ecu --can-if "$CAN_IF" --display-link "$DISPLAY_LINK" "${DURATION_ARG[@]}" &
PIDS+=($!)
sleep 0.3

echo "[run_demo] starting sensor_ecu"
./build/sensor_ecu --can-if "$CAN_IF" --sensor-link "$SENSOR_LINK" "${DURATION_ARG[@]}" &
PIDS+=($!)
sleep 0.3

if [[ "$HEADLESS" == "1" ]]; then
    echo "[run_demo] starting scenario_player with $SCENARIO"
    python3 tools/hmi/scenario_player.py --scenario "$SCENARIO" --link "$SENSOR_LINK" --loop --verbose &
    PIDS+=($!)
else
    echo "[run_demo] starting vehicle_hmi"
    python3 tools/hmi/vehicle_hmi.py --sensor-link "$SENSOR_LINK" --display-link "$DISPLAY_LINK" --can-if "$CAN_IF" &
    PIDS+=($!)
fi

echo "[run_demo] running. Watch the logs above, the HMI panel, and 'bash tools/inspect_can.sh'."
wait
