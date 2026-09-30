# Simplified AUTOSAR Classic Vehicle Speed Monitor (S32K144 hardware build)

## What this is

An educational, AUTOSAR-Classic-**inspired** implementation of a two-ECU
vehicle speed/cluster system running as real firmware on two NXP S32K144
boards, communicating over a real CAN bus. It reproduces the main
architectural boundaries, data flow, naming style, static configuration
approach, and development discipline of AUTOSAR Classic Platform.

**This project does NOT claim AUTOSAR conformance**, does not replace a
licensed BSW stack, and does not pretend that its hand-written
configuration is schema-compliant ARXML. No document in this repository
claims production AUTOSAR or ISO 26262 compliance. See
`AUTOSAR_CLASSIC_PROJECT_SPEC_v3.0.md` for the full specification this
repository implements, including every `REQ-*` requirement.

**This tree is hardware-only and has no automated tests.** The project
started as an Ubuntu host simulation (two processes, a virtual CAN
interface, a Tkinter HMI, and a 26-test suite) — that version is frozen
and still fully working, standalone, under [`legacy-sim/`](legacy-sim/).
This tree replaced that simulation with real S32K144 firmware; see
section 14 of the v3.0 spec for how it's verified instead (flashing real
hardware, not `ctest`).

## Architecture in one paragraph

Two independent S32K144 firmware images (`sensor_ecu`, `cluster_ecu`) run
a simplified Classic Platform stack — Application SW-Cs, a generated-style
RTE, simplified `Com`/`PduR`/`CanIf`, BSW mode management
(`BswM`/`ComM`/`CanSM`), a register-level MCAL (`Mcu`, `Port`, `Gpt`,
`Adc`, `Dio`, `Can`, `Display`), and an `IoHwAb` ECU Abstraction layer —
communicating over a real CAN bus with Classic CAN frames. `SensorEcu`
reads a real wheel-speed sensor (ADC) and ignition line (DIO); `ClusterEcu`
renders its state as text over LPUART1, viewable in any serial terminal.
See `docs/ARCHITECTURE.md` for the full layer diagram and ownership table.

## Status

**Hardware-only pivot (v3.0), just landed, not yet build- or
hardware-verified in this environment** (no `arm-none-eabi-gcc` toolchain
or physical board was available to verify against while making this
change — see `docs/IMPLEMENTATION_STATUS.md`). Before treating this as
working:

1. Build with the real toolchain (see "Build" below) and fix anything that
   doesn't compile.
2. Flash both boards and run the bring-up checklist in the spec's section 14.
3. Fix the known-open correctness issues carried over from before this
   pivot — see `docs/IMPLEMENTATION_STATUS.md`'s Phase 9 section
   (`BswM` shutdown action list not deinitializing drivers, `BswM_DeInit`
   no-op after a failed init, `EcuM_StartupTwo` claiming `RUN` before
   `BswM_Init` succeeds, `Os.c` reaching into `BswM` directly,
   `ComM_ModeType` defined twice, plus a few MCAL-level findings). These
   matter more now than before, since there is no test suite left to catch
   a regression.

Everything through v2.0 (the module split and the register-level MCAL
itself) was implemented and verified against the Ubuntu simulation, which
remains true and unchanged in `legacy-sim/`.

## Build

Requires an `arm-none-eabi-gcc` cross toolchain on `PATH` (not provided by
this repository).

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/s32k144_toolchain.cmake
cmake --build build --parallel
```

Produces `build/sensor_ecu(.elf)` / `build/cluster_ecu(.elf)`, plus
post-build `.hex` and `.bin` artifacts next to each. There is no
`ctest` target in this tree.

## Flash and run

Flashing is left to your own toolchain (J-Link Commander, OpenOCD,
PEmicro, or an IDE's flash action against the `.hex`/`.elf`) — this
project doesn't mandate one. After flashing both boards:

1. Wire a real CAN bus (CAN-H/CAN-L + termination) between the two
   boards' FlexCAN0 pins.
2. Wire a wheel-speed signal (or bench voltage source) into `SensorEcu`'s
   ADC0 pin and an ignition signal into its DIO pin.
3. Connect `ClusterEcu`'s LPUART1 (PTC6/PTC7 — the S32K144-EVB's OpenSDA
   USB-serial port) to a PC and open a serial terminal at ~9600 baud.

See the v3.0 spec's section 14 for the full bring-up checklist (normal
exchange, overspeed hysteresis, timeout/recovery, sensor-line loss).

## Repository layout

See section 12 of `AUTOSAR_CLASSIC_PROJECT_SPEC_v3.0.md` for the full
rationale. Short version:

| Directory | Contents |
|---|---|
| `platform/` | Fixed-width base types shared by every layer; `platform/s32k144/` holds the linker script, startup code, and register headers |
| `bsw/services/` | `Det`, `Dem`, `Logger`, `EcuM`, `BswM`, `ComM` |
| `bsw/os/` | `Os` (the scheduler) and `Os_Counter` (the OS's own tick-derived counter, reads only `SysTimer`, never `Gpt`) |
| `bsw/communication/` | `Com`, `PduR`, `CanIf`, `CanSM` |
| `bsw/ecu_abstraction/` | `IoHwAb` |
| `bsw/mcal/` | `Mcu`, `Port`, `Gpt`, `SysTimer`, `Adc`, `Dio`, `Display`, `Can` — each a `.h` interface plus one `_S32K144.c` register-level implementation; the only directory allowed to touch a peripheral register |
| `asw/` | Application SW-Cs |
| `rte/` | Generated-style, ECU-specific RTE code |
| `ecu/` | The two ECUs' bare-metal `main.c` entry points and their static configuration |
| `legacy-sim/` | Frozen Ubuntu-simulation snapshot (own `CMakeLists.txt`, own spec, own 26-test suite) — see `legacy-sim/README.md` |

## Non-conformance disclaimer

This is an educational project. It is **not** a certified AUTOSAR Classic
Platform implementation, is **not** ISO 26262-qualified, and its
`ComProtection` CRC/counter scheme is explicitly not a complete AUTOSAR E2E
Profile. Any AUTOSAR-shaped API in this repository is a simplification
documented next to its definition and in the specification document.
