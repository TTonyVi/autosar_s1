# Simplified AUTOSAR Classic Vehicle Speed Monitor

## What this is

An educational, AUTOSAR-Classic-**inspired** implementation of a two-ECU
vehicle speed/cluster demo running on Ubuntu, driven by a Tkinter HMI that
plays the role of the vehicle hardware. It reproduces the main
architectural boundaries, data flow, naming style, static configuration
approach, and development discipline of AUTOSAR Classic Platform.

**This project does NOT claim AUTOSAR conformance**, does not replace a
licensed BSW stack, and does not pretend that its hand-written
configuration is schema-compliant ARXML. No document in this repository
claims production AUTOSAR or ISO 26262 compliance. See
`AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md` for the full specification this
repository implements, including every `REQ-*` requirement and the phase
plan.

## Architecture in one paragraph

Two single-threaded ECU processes (`sensor_ecu`, `cluster_ecu`) run a
simplified Classic Platform stack — Application SW-Cs, a generated-style
RTE, simplified `Com`/`PduR`/`CanIf`, a simplified MCAL (`Mcu`, `Port`,
`Gpt`, `Adc`, `Dio`, `Can`, `Display`), and an `IoHwAb` ECU Abstraction
layer — communicating over `vcan0` with Classic CAN frames. Sensor input
(wheel speed, ignition) enters only through a virtual hardware link at the
bottom of the stack, never invented inside the ECU. A third process, the
Tkinter HMI (`tools/hmi/vehicle_hmi.py`), plays the role of the vehicle's
physical sensors and instrument panel; it is test equipment, not part of
the ECU software. See `docs/ARCHITECTURE.md` for the full layer diagram and
ownership table.

## Status

This repository is built phase-by-phase per section 16 of the
specification. See `docs/IMPLEMENTATION_STATUS.md` for what is currently
implemented. As of this writing, **Phase 0 (repository and documentation
baseline) is complete**; the ECU executables are placeholders that print a
version string and exit.

## Ubuntu prerequisites

Targets Ubuntu 22.04 LTS or 24.04 LTS, x86_64. No other distribution is
claimed or tested.

```bash
sudo apt update
sudo apt install -y build-essential cmake clang git can-utils python3 python3-tk
```

`python3-tk` is required for the HMI (Phase 7); on a minimal/server install
or container, `import tkinter` fails without it.

### Virtual CAN interface

```bash
./tools/setup_vcan.sh
```

This idempotently loads the `vcan` kernel module, creates `vcan0` if
missing, and brings it up; it is safe to re-run. `vcan` is a kernel module,
so inside a container you need `--cap-add=NET_ADMIN` (or an already-up
`vcan0` provided by the host).

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run

Three processes, any order — the system is designed to tolerate any of
them starting, stopping, or restarting independently:

```bash
./build/cluster_ecu --can-if vcan0 --display-link /tmp/vhw_cluster_display.sock
./build/sensor_ecu  --can-if vcan0 --sensor-link  /tmp/vhw_sensor.sock
python3 tools/hmi/vehicle_hmi.py
```

(Command-line argument parsing and the HMI itself are introduced in later
phases; see `docs/IMPLEMENTATION_STATUS.md`.)

For a headless/CI run, `tools/hmi/scenario_player.py` (Phase 7) replays a
deterministic CSV scenario onto the sensor line in place of the GUI.

## Repository layout

See section 12 of `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md` for the full
rationale. Short version:

| Directory | Contents |
|---|---|
| `platform/` | Fixed-width base types shared by every layer |
| `bsw/services/` | `Det`, `Dem`, `EcuM`, `Os` |
| `bsw/communication/` | `Com`, `PduR`, `CanIf` |
| `bsw/ecu_abstraction/` | `IoHwAb` |
| `bsw/mcal/` | `Mcu`, `Port`, `Gpt`, `Adc`, `Dio`, `Display`, `Can`, `VirtualHw` — the **only** directory allowed to contain host-specific code (sockets, `clock_gettime`) |
| `asw/` | Application SW-Cs |
| `rte/` | Generated-style, ECU-specific RTE code |
| `ecu/` | The two ECU `main()` entry points and their static configuration |
| `tests/` | Unit and integration tests |
| `tools/hmi/` | The Tkinter HMI and headless scenario player (Python, standard library only, never linked into an ECU) |
| `scenarios/` | CSV scenario files for the headless player |

## Non-conformance disclaimer

This is an educational project. It is **not** a certified AUTOSAR Classic
Platform implementation, is **not** ISO 26262-qualified, and its
`ComProtection` CRC/counter scheme is explicitly not a complete AUTOSAR E2E
Profile. Any AUTOSAR-shaped API in this repository is a simplification
documented next to its definition and in the specification document.
