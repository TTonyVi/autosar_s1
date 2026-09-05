#!/usr/bin/env bash
#
# setup_vcan.sh
#
# Idempotently brings up the vcan0 virtual CAN interface used by both ECUs
# and the HMI (AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 15.1).
# Safe to re-run: it succeeds whether vcan0 already exists/is already up or
# not, and it never fails just because a previous run already did the work.
#
# Requires root privileges (sudo) for the vcan module load, link creation,
# and bringing the link up. Inside a container this additionally requires
# --cap-add=NET_ADMIN (or an equivalent host-side vcan0 already present).

set -euo pipefail

IFACE="${1:-vcan0}"

log() {
    printf '[setup_vcan] %s\n' "$1"
}

if ! command -v ip >/dev/null 2>&1; then
    echo "[setup_vcan] ERROR: 'ip' command not found (install iproute2)." >&2
    exit 1
fi

log "loading vcan kernel module (no-op if already loaded)"
sudo modprobe vcan

if ip link show "$IFACE" >/dev/null 2>&1; then
    log "interface '$IFACE' already exists"
else
    log "creating interface '$IFACE'"
    sudo ip link add dev "$IFACE" type vcan
fi

if ip -details link show "$IFACE" | grep -q '<.*UP.*>'; then
    log "interface '$IFACE' already up"
else
    log "bringing interface '$IFACE' up"
    sudo ip link set up "$IFACE"
fi

log "final state:"
ip -details link show "$IFACE"
