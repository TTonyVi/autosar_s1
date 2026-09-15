#!/usr/bin/env bash
# inspect_can.sh - show what is actually on the bus.
#
# AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 15.8: the raw bus is one of
# three independent observation surfaces, and the only one that proves a
# value really crossed the network rather than being claimed by a log.
#
# Prefers `candump` from can-utils when it is installed, because that is
# the tool a CAN engineer already knows; falls back to this project's own
# decoder, which additionally decodes the two PDUs into signals.
set -euo pipefail

IFACE="${1:-vcan0}"

if ! ip link show "$IFACE" >/dev/null 2>&1; then
    echo "[inspect_can] interface '$IFACE' does not exist" >&2
    echo "[inspect_can] run: bash tools/setup_vcan.sh" >&2
    exit 1
fi

echo "[inspect_can] observing '$IFACE' read-only, Ctrl+C to stop"

if command -v candump >/dev/null 2>&1; then
    echo "[inspect_can] using candump (raw frames)"
    exec candump -tz "$IFACE"
else
    echo "[inspect_can] candump not found (apt install can-utils); using the project decoder"
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    exec python3 "$SCRIPT_DIR/hmi/can_monitor.py" --can-if "$IFACE"
fi
