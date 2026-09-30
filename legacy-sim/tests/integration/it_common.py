"""Shared harness for the section 14.2 integration tests.

These start the REAL `sensor_ecu`/`cluster_ecu` binaries as subprocesses
and drive them through the REAL virtual hardware links and the REAL
`vcan0`, exactly like a human running the demo — no test-only shortcut
inside the ECU (REQ-ARCH-007: any shortcut lives here, under `tests/`).

Not linked into any ECU target; standard library only, matching the same
constraint the HMI tools observe, since this is the same kind of external
observer.
"""
from __future__ import annotations

import os
import signal
import socket
import struct
import subprocess
import sys
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
# IT_BUILD_DIR lets the same tests run against a different build tree (e.g.
# build-asan) without duplicating a single line of test logic -- used for
# the section 14.3 sanitizer run (docs/IMPLEMENTATION_STATUS.md, Phase 8).
_BUILD_DIR = os.environ.get("IT_BUILD_DIR", "build")
SENSOR_ECU = os.path.join(REPO_ROOT, _BUILD_DIR, "sensor_ecu")
CLUSTER_ECU = os.path.join(REPO_ROOT, _BUILD_DIR, "cluster_ecu")

CAN_FRAME_FORMAT = "=IB3x8s"
CAN_FRAME_SIZE = struct.calcsize(CAN_FRAME_FORMAT)
CAN_SFF_MASK = 0x000007FF

VEHICLE_STATUS_ID = 0x180
CLUSTER_STATUS_ID = 0x181

CLUSTER_STATE_NAMES = {0: "OFF", 1: "NORMAL", 2: "OVERSPEED", 3: "COMM_FAULT"}


class ItFailure(AssertionError):
    pass


def require_binaries() -> None:
    for path in (SENSOR_ECU, CLUSTER_ECU):
        if not os.path.isfile(path) or not os.access(path, os.X_OK):
            raise ItFailure(f"missing built executable: {path} (build first)")


def require_vcan(interface: str = "vcan0") -> None:
    result = subprocess.run(["ip", "link", "show", interface], capture_output=True, text=True)
    if result.returncode != 0:
        raise ItFailure(f"{interface} does not exist; run tools/setup_vcan.sh first")


class Ecu:
    """One running ECU subprocess, torn down on __exit__ with SIGTERM
    (matching REQ-ECUM-003's own expected signal, not SIGKILL)."""

    def __init__(self, binary: str, args: list[str], name: str) -> None:
        self.name = name
        self.log_path = f"/tmp/it_{name}_{os.getpid()}.log"
        self.log_file = open(self.log_path, "w")
        self.process = subprocess.Popen(
            [binary, *args], stdout=self.log_file, stderr=subprocess.STDOUT, cwd=REPO_ROOT
        )

    def stop(self, timeout: float = 3.0) -> None:
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGTERM)
            try:
                self.process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=1.0)
        self.log_file.close()

    def read_log(self) -> str:
        with open(self.log_path, "r") as handle:
            return handle.read()

    def is_alive(self) -> bool:
        return self.process.poll() is None

    def __enter__(self) -> "Ecu":
        return self

    def __exit__(self, *exc) -> None:
        self.stop()


def cleanup_link_files(*paths: str) -> None:
    for path in paths:
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass


class CanObserver:
    """Read-only trace of vcan0, decoding the two PDUs this project owns."""

    def __init__(self, interface: str = "vcan0") -> None:
        self.sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self.sock.bind((interface,))
        self.sock.settimeout(0.05)

    def close(self) -> None:
        self.sock.close()

    def drain(self, duration_s: float) -> list[tuple[int, bytes, float]]:
        """Collects every frame seen over `duration_s`, timestamped."""
        deadline = time.monotonic() + duration_s
        frames = []
        while time.monotonic() < deadline:
            try:
                raw = self.sock.recv(CAN_FRAME_SIZE)
            except socket.timeout:
                continue
            if len(raw) < CAN_FRAME_SIZE:
                continue
            can_id, _dlc, data = struct.unpack(CAN_FRAME_FORMAT, raw)
            frames.append((can_id & CAN_SFF_MASK, bytes(data), time.monotonic()))
        return frames

    def wait_for(self, can_id: int, predicate, timeout_s: float):
        """Returns the first matching frame's data, or None on timeout."""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            try:
                raw = self.sock.recv(CAN_FRAME_SIZE)
            except socket.timeout:
                continue
            if len(raw) < CAN_FRAME_SIZE:
                continue
            frame_id, _dlc, data = struct.unpack(CAN_FRAME_FORMAT, raw)
            if (frame_id & CAN_SFF_MASK) == can_id and predicate(bytes(data)):
                return bytes(data)
        return None


def decode_vehicle_status(data: bytes) -> dict:
    return {
        "speed_raw": data[0] | (data[1] << 8),
        "ignition": bool(data[2] & 0x01),
        "counter": data[3] & 0x0F,
    }


def decode_cluster_status(data: bytes) -> dict:
    return {
        "state": data[0],
        "speed_raw": data[1] | (data[2] << 8),
        "flags": data[3] & 0x07,
        "counter": data[4] & 0x0F,
    }


class SensorLineSender:
    """Sends directly onto the sensor line, standing in for the HMI/scenario
    player (section 15.5.2: the ECU binds, this side sends)."""

    def __init__(self, link_path: str) -> None:
        self.link_path = link_path
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self._seq = 0

    def send(self, counts: int, ignition_on: bool) -> None:
        self._seq += 1
        message = f"ADC0={counts};DIO0={1 if ignition_on else 0};SEQ={self._seq}"
        try:
            self.sock.sendto(message.encode("ascii"), self.link_path)
        except OSError:
            # The consumer (sensor_ecu) may not have bound yet, or may have
            # been stopped by the test on purpose (IT-02/IT-06). Matches
            # REQ-HMI-005/REQ-VHW-005: an absent peer on a wire is a normal
            # condition for the producer side too, never a reason to raise.
            pass

    def close(self) -> None:
        self.sock.close()


def speed_kph_to_counts(speed_kph: float) -> int:
    speed_raw = round(speed_kph * 100.0)
    return max(0, min(4095, round(speed_raw * 4095 / 25000)))


class DisplayLineListener:
    """Binds the display line, standing in for the HMI (section 15.5.2:
    the HMI binds the display line, ClusterEcu sends)."""

    def __init__(self, link_path: str) -> None:
        cleanup_link_files(link_path)
        self.link_path = link_path
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.sock.bind(link_path)
        self.sock.settimeout(0.05)

    def close(self) -> None:
        self.sock.close()
        cleanup_link_files(self.link_path)

    def wait_for(self, predicate, timeout_s: float):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            try:
                payload = self.sock.recv(256).decode("ascii", errors="replace")
            except socket.timeout:
                continue
            fields = dict(p.split("=", 1) for p in payload.strip().split(";") if "=" in p)
            if predicate(fields):
                return fields
        return None

    def drain_latest(self, duration_s: float):
        deadline = time.monotonic() + duration_s
        latest = None
        while time.monotonic() < deadline:
            try:
                payload = self.sock.recv(256).decode("ascii", errors="replace")
            except socket.timeout:
                continue
            latest = dict(p.split("=", 1) for p in payload.strip().split(";") if "=" in p)
        return latest


# --- CRC-8/AUTOSAR (spec section 6.5), re-implemented independently from
# ComProtection.c so IT-03 can construct frames the ECU never produced
# itself, including deliberately wrong ones. Same parameters: polynomial
# 0x1D, init 0xFF, MSB-first (no reflection), final XOR 0xFF. ---
_CRC8_POLY = 0x1D
_CRC8_INIT = 0xFF
_CRC8_XOROUT = 0xFF


def _crc8_update_byte(crc: int, data_byte: int) -> int:
    crc = (crc ^ data_byte) & 0xFF
    for _ in range(8):
        if crc & 0x80:
            crc = ((crc << 1) ^ _CRC8_POLY) & 0xFF
        else:
            crc = (crc << 1) & 0xFF
    return crc


def compute_crc8(protection_data_id: int, payload: bytes, crc_byte_index: int) -> int:
    """Mirrors ComProtection_ComputeCrc8: the CRC byte's own prior content
    is zeroed before computing, matching Com.c's own convention."""
    working = bytearray(payload)
    if 0 <= crc_byte_index < len(working):
        working[crc_byte_index] = 0
    crc = _CRC8_INIT
    crc = _crc8_update_byte(crc, (protection_data_id >> 8) & 0xFF)
    crc = _crc8_update_byte(crc, protection_data_id & 0xFF)
    for byte in working:
        crc = _crc8_update_byte(crc, byte)
    return crc ^ _CRC8_XOROUT


def build_vehicle_status_frame(speed_raw: int, ignition_on: bool, counter: int,
                               corrupt_crc: bool = False) -> bytes:
    """Section 6.3 payload layout, section 6.5 protection. `corrupt_crc`
    deliberately flips the CRC to build a frame IT-03 needs but no
    correctly-functioning ECU would ever produce."""
    frame = bytearray(8)
    frame[0] = speed_raw & 0xFF
    frame[1] = (speed_raw >> 8) & 0xFF
    frame[2] = 1 if ignition_on else 0
    frame[3] = counter & 0x0F
    frame[4] = compute_crc8(0x0180, bytes(frame), crc_byte_index=4)
    if corrupt_crc:
        frame[4] ^= 0xFF
    return bytes(frame)


class RawCanSender:
    """Sends a raw frame directly onto vcan0 -- standing in for a
    malfunctioning or hostile node, which is exactly what IT-03 needs to
    inject (a real ECU never produces a bad-CRC frame on purpose)."""

    def __init__(self, interface: str = "vcan0") -> None:
        self.sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self.sock.bind((interface,))

    def send(self, can_id: int, data: bytes) -> None:
        padded = data + bytes(8 - len(data))
        frame = struct.pack(CAN_FRAME_FORMAT, can_id & CAN_SFF_MASK, len(data), padded)
        self.sock.send(frame)

    def close(self) -> None:
        self.sock.close()


def run_case(name: str, fn) -> bool:
    print(f"--- {name} ---")
    try:
        fn()
    except ItFailure as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return False
    except Exception as error:  # noqa: BLE001 - report unexpected errors as failures too
        print(f"ERROR: {type(error).__name__}: {error}", file=sys.stderr)
        return False
    print(f"PASS: {name}")
    return True
