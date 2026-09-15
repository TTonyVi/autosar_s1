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

This repository was built phase-by-phase per section 16 of the
specification. See `docs/IMPLEMENTATION_STATUS.md` for the full
phase-by-phase record; **all 8 phases (plus the between-4-and-5
SysTimer/Os_Counter split) are complete**: both ECU executables run their
full lifecycle over real `vcan0`, all section 14.1 unit tests and all
seven section 14.2 integration tests (IT-01 through IT-07) pass, the
project builds and passes its full test suite under AddressSanitizer and
UndefinedBehaviorSanitizer with zero diagnostics, and `REQ-ARCH-012`'s
stub-MCAL build is verified. `docs/REQUIREMENTS_TRACEABILITY.md` covers
every `REQ-*` requirement in the specification.

## Ubuntu prerequisites

Targets Ubuntu 22.04 LTS or 24.04 LTS, x86_64. No other distribution is
claimed or tested. (Developed and verified on Ubuntu 20.04 with GCC 9.4.0;
`clang` is optional — this project builds and tests with GCC only.)

```bash
sudo apt update
sudo apt install -y build-essential cmake git can-utils python3 python3-tk
```

`python3-tk` is required for the HMI; on a minimal/server install or
container, `import tkinter` fails without it.

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
ctest --test-dir build --output-on-failure -LE integration   # fast: unit tests only
ctest --test-dir build --output-on-failure -L integration    # slower: real ECU subprocesses
```

`test_can` and every integration test (`IT-*`) open a real socket on
`vcan0` and require the "Virtual CAN interface" step above to have run
first; every other unit test target has no such prerequisite. A bare
`ctest` (no `-L`/`-LE` filter) runs everything.

Two additional, optional verification builds exist alongside the normal
one (see `docs/TEST_PLAN.md` for full commands): a sanitizer build
(`-fsanitize=address,undefined`) and a `REQ-ARCH-012` stub-MCAL build
(`-DAUTOSAR_MCAL_VARIANT=stub`, swapping only the two files that touch a
real socket for in-memory-buffer equivalents, proving a microcontroller
MCAL port would need no change outside `bsw/mcal/`).

## Run

Three processes, any order — the system is designed to tolerate any of
them starting, stopping, or restarting independently:

```bash
./build/cluster_ecu --can-if vcan0 --display-link /tmp/vhw_cluster_display.sock
./build/sensor_ecu  --can-if vcan0 --sensor-link  /tmp/vhw_sensor.sock
python3 tools/hmi/vehicle_hmi.py
```

Or let `tools/run_demo.sh` start all three (a convenience, never a
requirement — each executable stays fully usable on its own):

```bash
bash tools/run_demo.sh                 # HMI demo, needs a display
bash tools/run_demo.sh --headless      # scenario_player instead of the GUI
```

For a headless/CI run without a display, `tools/hmi/scenario_player.py`
replays a deterministic CSV scenario (`scenarios/default.csv`,
`scenarios/timeout_recovery.csv`) onto the sensor line in place of the GUI:

```bash
python3 tools/hmi/scenario_player.py --scenario scenarios/default.csv \
    --link /tmp/vhw_sensor.sock
```

To watch the raw bus independently of either ECU's own logs:

```bash
bash tools/inspect_can.sh vcan0   # candump if installed, else the project's own decoder
```

`tools/hmi/hw_link.py` sends one manual datagram onto either virtual
hardware link, useful for poking a single value without the GUI or a full
scenario:

```bash
python3 tools/hmi/hw_link.py --link /tmp/vhw_sensor.sock --speed 120.0 --ignition on
```

## Repository layout

See section 12 of `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md` for the full
rationale. Short version:

| Directory | Contents |
|---|---|
| `platform/` | Fixed-width base types shared by every layer |
| `bsw/services/` | `Det`, `Dem`, `Logger`, `EcuM` |
| `bsw/os/` | `Os` (the scheduler) and `Os_Counter` (the OS's own tick-derived counter, reads only `SysTimer`, never `Gpt`) |
| `bsw/communication/` | `Com`, `PduR`, `CanIf` |
| `bsw/ecu_abstraction/` | `IoHwAb` |
| `bsw/mcal/` | `Mcu`, `Port`, `Gpt`, `SysTimer`, `Adc`, `Dio`, `Display`, `Can`, `VirtualHw` — the **only** directory allowed to contain host-specific code (sockets, `clock_gettime`); also holds the `Can_Stub.c`/`VirtualHw_Stub.c` `REQ-ARCH-012` proof variant |
| `asw/` | Application SW-Cs |
| `rte/` | Generated-style, ECU-specific RTE code |
| `ecu/` | The two ECU `main()` entry points and their static configuration |
| `tests/unit/`, `tests/fixtures/` | Unit tests and their test doubles |
| `tests/integration/` | The seven `IT-*` end-to-end tests, driving real ECU subprocesses |
| `tools/hmi/` | The Tkinter HMI, headless scenario player, and CAN trace tool (Python, standard library only, never linked into an ECU) |
| `scenarios/` | CSV scenario files for the headless player |

## Non-conformance disclaimer

This is an educational project. It is **not** a certified AUTOSAR Classic
Platform implementation, is **not** ISO 26262-qualified, and its
`ComProtection` CRC/counter scheme is explicitly not a complete AUTOSAR E2E
Profile. Any AUTOSAR-shaped API in this repository is a simplification
documented next to its definition and in the specification document.
