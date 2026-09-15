#!/usr/bin/env python3
"""vehicle_hmi.py — the virtual hardware rig and instrument panel.

AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 15.6. This is the
interactive face of the project and the intended way to run the demo: the
slider on the left is the source of the demonstration and the panel on the
right is the result of it, so a reader can watch one value cross the whole
architecture in a single window.

What this program is NOT (REQ-ARCH-011, REQ-HMI-004): it never writes to
`vcan0`, never reads ECU memory or logs, and never imports or links ECU
source. Everything the right panel shows arrives on the display line;
everything the left panel produces leaves on the sensor line.

Threading (REQ-HMI-002): every socket lives in a worker thread and talks to
the GUI through a queue. Tkinter widgets are touched only from the main
thread, refreshed by `after()` at a fixed 100 ms period.

Example:
    python3 tools/hmi/vehicle_hmi.py
    python3 tools/hmi/vehicle_hmi.py --sensor-link /tmp/vhw_sensor.sock --no-can-trace
"""
# See hw_link.py for why annotations are made lazy.
from __future__ import annotations

import argparse
import itertools
import os
import queue
import socket
import struct
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk

# REQ-HMI-003: counts = round(kph * 100 * 4095 / 25000), the inverse of the
# IoHwAb scaling. The counts are shown in the interface so the conversion
# is not hidden from the reader.
ADC_FULL_SCALE_COUNTS = 4095
SPEED_RAW_FULL_SCALE = 25000

SENSOR_PERIOD_S = 0.100  # REQ-VHW-007 keepalive
GUI_REFRESH_MS = 100  # REQ-HMI-002 upper bound
CAN_TRACE_CAPACITY = 200  # section 15.6.1
DISPLAY_STALE_MS = 500

CAN_FRAME_FORMAT = "=IB3x8s"
CAN_FRAME_SIZE = struct.calcsize(CAN_FRAME_FORMAT)
CAN_SFF_MASK = 0x000007FF

LAMP_OFF = "#3a3a3a"
LAMP_RED = "#e04040"
LAMP_AMBER = "#e0a030"
LAMP_GREEN = "#40c060"


def speed_kph_to_counts(speed_kph: float) -> int:
    speed_raw = round(speed_kph * 100.0)
    counts = round(speed_raw * ADC_FULL_SCALE_COUNTS / SPEED_RAW_FULL_SCALE)
    return max(0, min(ADC_FULL_SCALE_COUNTS, counts))


class SensorRigSender(threading.Thread):
    """Produces the sensor line. The ECU binds it; this side only sends.

    REQ-HMI-003: sends on every change and at least every 100 ms.
    REQ-HMI-007: the only fault injection offered is the one the modelled
    hardware can actually do — stop transmitting, or send out-of-range
    counts.
    """

    def __init__(self, link_path: str, events: queue.Queue) -> None:
        super().__init__(daemon=True)
        self._link_path = link_path
        self._events = events
        self._lock = threading.Lock()
        self._speed_kph = 0.0
        self._ignition = False
        self._connected = True
        self._out_of_range = False
        # NOT named _stop: threading.Thread already has a private _stop()
        # method, and shadowing it breaks Thread.join().
        self._stop_event = threading.Event()
        self._sequence = itertools.count(1)
        self._sent = 0

    def set_state(self, speed_kph: float, ignition: bool, connected: bool, out_of_range: bool) -> None:
        with self._lock:
            self._speed_kph = speed_kph
            self._ignition = ignition
            self._connected = connected
            self._out_of_range = out_of_range

    def stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.setblocking(False)
        try:
            while not self._stop_event.is_set():
                with self._lock:
                    speed_kph = self._speed_kph
                    ignition = self._ignition
                    connected = self._connected
                    out_of_range = self._out_of_range

                if connected:
                    counts = 60000 if out_of_range else speed_kph_to_counts(speed_kph)
                    message = f"ADC0={counts};DIO0={1 if ignition else 0};SEQ={next(self._sequence)}"
                    try:
                        sock.sendto(message.encode("ascii"), self._link_path)
                        self._sent += 1
                        self._events.put(("sensor", {"counts": counts, "sent": self._sent, "peer": True}))
                    except OSError:
                        # REQ-HMI-005: an absent ECU is shown, never fatal.
                        self._events.put(("sensor", {"counts": counts, "sent": self._sent, "peer": False}))
                self._stop_event.wait(SENSOR_PERIOD_S)
        finally:
            sock.close()


class DisplayLineReceiver(threading.Thread):
    """Consumes the display line. The HMI binds it (section 15.5.2)."""

    def __init__(self, link_path: str, events: queue.Queue) -> None:
        super().__init__(daemon=True)
        self._link_path = link_path
        self._events = events
        # NOT named _stop: threading.Thread already has a private _stop()
        # method, and shadowing it breaks Thread.join().
        self._stop_event = threading.Event()

    def stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        # REQ-ECUM-006's counterpart on this side: a stale socket file from
        # a previous run is handled here, not by asking a human to rm it.
        try:
            os.unlink(self._link_path)
        except FileNotFoundError:
            pass
        except OSError as error:
            self._events.put(("display_error", str(error)))
            return

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        try:
            sock.bind(self._link_path)
        except OSError as error:
            self._events.put(("display_error", str(error)))
            sock.close()
            return
        sock.settimeout(0.2)

        try:
            while not self._stop_event.is_set():
                try:
                    payload = sock.recv(256)
                except socket.timeout:
                    continue
                except OSError:
                    continue
                self._events.put(("display", payload.decode("ascii", errors="replace")))
        finally:
            sock.close()
            try:
                os.unlink(self._link_path)
            except OSError:
                pass


class CanTraceReader(threading.Thread):
    """Read-only observation of vcan0 (REQ-HMI-004)."""

    def __init__(self, interface: str, events: queue.Queue) -> None:
        super().__init__(daemon=True)
        self._interface = interface
        self._events = events
        # NOT named _stop: threading.Thread already has a private _stop()
        # method, and shadowing it breaks Thread.join().
        self._stop_event = threading.Event()

    def stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        try:
            sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
            sock.bind((self._interface,))
        except OSError as error:
            self._events.put(("can_error", str(error)))
            return
        sock.settimeout(0.2)

        try:
            while not self._stop_event.is_set():
                try:
                    frame = sock.recv(CAN_FRAME_SIZE)
                except socket.timeout:
                    continue
                except OSError:
                    continue
                if len(frame) < CAN_FRAME_SIZE:
                    continue
                raw_id, dlc, data = struct.unpack(CAN_FRAME_FORMAT, frame)
                self._events.put(("can", (raw_id & CAN_SFF_MASK, dlc, bytes(data[:dlc]))))
        finally:
            sock.close()


def decode_can(can_id: int, data: bytes) -> str:
    """Decoded from the spec's payload tables, never from ECU source."""
    if can_id == 0x180 and len(data) >= 5:
        speed = (data[0] | (data[1] << 8)) / 100.0
        return f"speed={speed:.2f} ign={'on' if data[2] & 1 else 'off'} cnt={data[3] & 0x0F}"
    if can_id == 0x181 and len(data) >= 6:
        names = {0: "OFF", 1: "NORMAL", 2: "OVERSPEED", 3: "COMM_FAULT"}
        speed = (data[1] | (data[2] << 8)) / 100.0
        return f"state={names.get(data[0], data[0])} speed={speed:.2f} flags=0x{data[3] & 7:02X} cnt={data[4] & 0x0F}"
    return ""


def parse_display_frame(text: str) -> dict:
    """Section 15.5.2: STATE=..;SPEED=..;FLAGS=0x..;T=.. , fixed order."""
    fields = {}
    for part in text.strip().split(";"):
        if "=" in part:
            key, value = part.split("=", 1)
            fields[key.strip()] = value.strip()
    return fields


class VehicleHmi:
    def __init__(self, root: tk.Tk, args) -> None:
        self.root = root
        self.events: queue.Queue = queue.Queue()

        self.last_display_time = 0.0
        self.last_display_fields: dict = {}
        self.sensor_counts = 0
        self.sensor_sent = 0
        self.sensor_peer = False
        self.last_sent_snapshot = 0
        self.last_rate_time = time.monotonic()
        self.send_rate = 0.0
        self.can_rows: list[str] = []

        root.title("Vehicle speed monitor — virtual hardware rig and cluster")
        root.configure(bg="#1e1e1e")

        container = ttk.Frame(root, padding=8)
        container.grid(row=0, column=0, sticky="nsew")
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)
        container.columnconfigure(0, weight=1)
        container.columnconfigure(1, weight=1)
        container.rowconfigure(1, weight=1)

        self._build_sensor_rig(container)
        self._build_cluster_panel(container)
        self._build_can_trace(container, enabled=not args.no_can_trace)

        self.sender = SensorRigSender(args.sensor_link, self.events)
        self.receiver = DisplayLineReceiver(args.display_link, self.events)
        self.sender.start()
        self.receiver.start()
        self.can_reader = None
        if not args.no_can_trace:
            self.can_reader = CanTraceReader(args.can_if, self.events)
            self.can_reader.start()

        self._push_sensor_state()
        self.root.protocol("WM_DELETE_WINDOW", self.shutdown)
        self.root.after(GUI_REFRESH_MS, self.refresh)

    # ---------------------------- layout ----------------------------

    def _build_sensor_rig(self, parent) -> None:
        frame = ttk.LabelFrame(parent, text="Sensor rig — produces stimuli", padding=8)
        frame.grid(row=0, column=0, rowspan=2, sticky="nsew", padx=(0, 6))

        ttk.Label(frame, text="Wheel speed (km/h)").grid(row=0, column=0, sticky="w")
        self.speed_var = tk.DoubleVar(value=0.0)
        self.speed_scale = ttk.Scale(frame, from_=0.0, to=250.0, orient="horizontal",
                                     variable=self.speed_var, command=self._on_speed_slider, length=280)
        self.speed_scale.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(0, 4))

        entry_row = ttk.Frame(frame)
        entry_row.grid(row=2, column=0, columnspan=2, sticky="w", pady=(0, 8))
        self.speed_entry = ttk.Entry(entry_row, width=8)
        self.speed_entry.insert(0, "0.00")
        self.speed_entry.grid(row=0, column=0)
        ttk.Button(entry_row, text="Set", command=self._on_speed_entry).grid(row=0, column=1, padx=4)
        ttk.Label(entry_row, text="step 0.5 on the slider").grid(row=0, column=2, padx=4)

        self.ignition_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(frame, text="Ignition ON", variable=self.ignition_var,
                        command=self._push_sensor_state).grid(row=3, column=0, sticky="w")

        self.connected_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(frame, text="Disconnect sensor line (fault injection)",
                        variable=self.connected_var,
                        command=self._push_sensor_state).grid(row=4, column=0, sticky="w", pady=(6, 0))

        self.out_of_range_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(frame, text="Send out-of-range counts (fault injection)",
                        variable=self.out_of_range_var,
                        command=self._push_sensor_state).grid(row=5, column=0, sticky="w")

        ttk.Separator(frame, orient="horizontal").grid(row=6, column=0, columnspan=2, sticky="ew", pady=8)

        self.counts_label = ttk.Label(frame, text="ADC counts sent: 0", font=("TkFixedFont", 11))
        self.counts_label.grid(row=7, column=0, sticky="w")
        self.rate_label = ttk.Label(frame, text="send rate: 0.0 /s", font=("TkFixedFont", 11))
        self.rate_label.grid(row=8, column=0, sticky="w")
        self.sensor_peer_label = ttk.Label(frame, text="sensor line: no ECU listening")
        self.sensor_peer_label.grid(row=9, column=0, sticky="w", pady=(4, 0))

    def _build_cluster_panel(self, parent) -> None:
        frame = ttk.LabelFrame(parent, text="Instrument cluster — consumes the display line", padding=8)
        frame.grid(row=0, column=1, sticky="nsew")

        self.big_speed = tk.Label(frame, text="---.--", font=("TkFixedFont", 44, "bold"),
                                  bg="#101010", fg="#e0e0e0", width=8)
        self.big_speed.grid(row=0, column=0, columnspan=3, pady=(0, 2))
        ttk.Label(frame, text="km/h").grid(row=1, column=0, columnspan=3)

        self.state_label = tk.Label(frame, text="no data", font=("TkDefaultFont", 14, "bold"),
                                    bg="#101010", fg="#a0a0a0", width=14)
        self.state_label.grid(row=2, column=0, columnspan=3, pady=6)

        self.lamps = {}
        for column, (key, text) in enumerate((("overspeed", "OVERSPEED"), ("timeout", "COMM"), ("integrity", "INTEGRITY"))):
            lamp = tk.Label(frame, text=text, bg=LAMP_OFF, fg="#101010", width=10,
                            font=("TkDefaultFont", 9, "bold"))
            lamp.grid(row=3, column=column, padx=2, pady=2)
            self.lamps[key] = lamp

        self.link_label = ttk.Label(frame, text="display line: waiting")
        self.link_label.grid(row=4, column=0, columnspan=3, sticky="w", pady=(8, 0))
        self.age_label = ttk.Label(frame, text="last frame age: -- ms", font=("TkFixedFont", 10))
        self.age_label.grid(row=5, column=0, columnspan=3, sticky="w")

    def _build_can_trace(self, parent, enabled: bool) -> None:
        frame = ttk.LabelFrame(parent, text="CAN trace — vcan0, read-only", padding=6)
        frame.grid(row=2, column=0, columnspan=2, sticky="nsew", pady=(8, 0))
        parent.rowconfigure(2, weight=1)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(1, weight=1)

        # A pause here only freezes what this window renders; it never
        # touches the socket or the ECUs (REQ-ARCH-011, REQ-HMI-004 — this
        # tool stays a passive, read-only observer of the bus either way).
        self.trace_paused = False
        control_row = ttk.Frame(frame)
        control_row.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 4))
        self.trace_pause_button = ttk.Button(control_row, text="Stop", command=self._toggle_trace_pause)
        self.trace_pause_button.pack(side="left")
        ttk.Button(control_row, text="Clear", command=self._clear_trace).pack(side="left", padx=(6, 0))
        self.trace_status_label = ttk.Label(control_row, text="live")
        self.trace_status_label.pack(side="left", padx=(8, 0))

        self.trace = tk.Text(frame, height=10, bg="#101010", fg="#c0c0c0",
                             font=("TkFixedFont", 9), state="disabled", wrap="none")
        self.trace.grid(row=1, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(frame, orient="vertical", command=self.trace.yview)
        scroll.grid(row=1, column=1, sticky="ns")
        self.trace.configure(yscrollcommand=scroll.set)
        if not enabled:
            self._append_trace("CAN trace disabled with --no-can-trace")

    def _toggle_trace_pause(self) -> None:
        self.trace_paused = not self.trace_paused
        self.trace_pause_button.configure(text="Resume" if self.trace_paused else "Stop")
        self.trace_status_label.configure(text="paused" if self.trace_paused else "live")

    def _clear_trace(self) -> None:
        self.can_rows.clear()
        self.trace.configure(state="normal")
        self.trace.delete("1.0", "end")
        self.trace.configure(state="disabled")

    # ---------------------------- input -----------------------------

    def _on_speed_slider(self, _value) -> None:
        stepped = round(self.speed_var.get() * 2.0) / 2.0  # 0.5 km/h step
        self.speed_var.set(stepped)
        self.speed_entry.delete(0, "end")
        self.speed_entry.insert(0, f"{stepped:.2f}")
        self._push_sensor_state()

    def _on_speed_entry(self) -> None:
        try:
            value = float(self.speed_entry.get())
        except ValueError:
            return
        value = max(0.0, min(250.0, value))
        self.speed_var.set(value)
        self.speed_entry.delete(0, "end")
        self.speed_entry.insert(0, f"{value:.2f}")
        self._push_sensor_state()

    def _push_sensor_state(self) -> None:
        self.sender.set_state(self.speed_var.get(), self.ignition_var.get(),
                              not self.connected_var.get(), self.out_of_range_var.get())

    # --------------------------- refresh ----------------------------

    def _append_trace(self, line: str) -> None:
        self.can_rows.append(line)
        if len(self.can_rows) > CAN_TRACE_CAPACITY:
            del self.can_rows[: len(self.can_rows) - CAN_TRACE_CAPACITY]
        self.trace.configure(state="normal")
        self.trace.delete("1.0", "end")
        self.trace.insert("1.0", "\n".join(self.can_rows))
        self.trace.see("end")
        self.trace.configure(state="disabled")

    def refresh(self) -> None:
        """Runs on the main thread only (REQ-HMI-002)."""
        drained = 0
        while drained < 500:
            try:
                kind, payload = self.events.get_nowait()
            except queue.Empty:
                break
            drained += 1

            if kind == "sensor":
                self.sensor_counts = payload["counts"]
                self.sensor_sent = payload["sent"]
                self.sensor_peer = payload["peer"]
            elif kind == "display":
                self.last_display_fields = parse_display_frame(payload)
                self.last_display_time = time.monotonic()
            elif kind == "can":
                if self.trace_paused:
                    continue
                can_id, dlc, data = payload
                hex_payload = " ".join(f"{b:02X}" for b in data)
                self._append_trace(f"0x{can_id:03X} [{dlc}] {hex_payload:<23s} {decode_can(can_id, data)}")
            elif kind in ("can_error", "display_error"):
                self._append_trace(f"{kind}: {payload}")

        self._refresh_sensor_labels()
        self._refresh_cluster_panel()
        self.root.after(GUI_REFRESH_MS, self.refresh)

    def _refresh_sensor_labels(self) -> None:
        now = time.monotonic()
        window = now - self.last_rate_time
        if window >= 1.0:
            self.send_rate = (self.sensor_sent - self.last_sent_snapshot) / window
            self.last_sent_snapshot = self.sensor_sent
            self.last_rate_time = now

        self.counts_label.configure(text=f"ADC counts sent: {self.sensor_counts}")
        self.rate_label.configure(text=f"send rate: {self.send_rate:4.1f} /s")
        if self.connected_var.get():
            self.sensor_peer_label.configure(text="sensor line: DISCONNECTED (fault injection)")
        elif self.sensor_peer:
            self.sensor_peer_label.configure(text="sensor line: ECU listening")
        else:
            self.sensor_peer_label.configure(text="sensor line: no ECU listening")

    def _refresh_cluster_panel(self) -> None:
        if not self.last_display_fields:
            self.link_label.configure(text="display line: waiting for the cluster ECU")
            self.age_label.configure(text="last frame age: -- ms")
            return

        age_ms = int((time.monotonic() - self.last_display_time) * 1000.0)
        alive = age_ms <= DISPLAY_STALE_MS
        self.age_label.configure(text=f"last frame age: {age_ms:4d} ms")
        self.link_label.configure(text=f"display line: {'alive' if alive else 'STALE'}")

        state = self.last_display_fields.get("STATE", "?")
        try:
            speed = int(self.last_display_fields.get("SPEED", "0")) / 100.0
            flags = int(self.last_display_fields.get("FLAGS", "0x00"), 16)
        except ValueError:
            return

        self.big_speed.configure(text=f"{speed:6.2f}" if alive else "---.--")
        self.state_label.configure(text=state, fg="#e0e0e0" if alive else "#808080")
        self.lamps["overspeed"].configure(bg=LAMP_RED if (alive and flags & 0x01) else LAMP_OFF)
        self.lamps["timeout"].configure(bg=LAMP_AMBER if (alive and flags & 0x02) else LAMP_OFF)
        self.lamps["integrity"].configure(bg=LAMP_AMBER if (alive and flags & 0x04) else LAMP_OFF)

    def shutdown(self) -> None:
        self.sender.stop()
        self.receiver.stop()
        if self.can_reader is not None:
            self.can_reader.stop()
        self.root.destroy()


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sensor-link", default="/tmp/vhw_sensor.sock")
    parser.add_argument("--display-link", default="/tmp/vhw_cluster_display.sock")
    parser.add_argument("--can-if", default="vcan0")
    parser.add_argument("--no-can-trace", action="store_true", help="skip the bus trace (no CAN permission needed)")
    args = parser.parse_args(argv)

    root = tk.Tk()
    VehicleHmi(root, args)
    # REQ-HMI-006: usable with neither ECU running, reconnecting on its own.
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
