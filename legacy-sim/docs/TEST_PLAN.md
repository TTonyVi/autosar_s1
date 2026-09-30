# Test plan

Source of truth for test scope is `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md`,
section 14. This document tracks what's actually implemented against that
plan; it does not restate the full spec.

## Unit tests (spec section 14.1)

| Test target | Status |
|---|---|
| `test_com_protection` | **Done** — all three section 6.5 golden CRC8 vectors, the CRC byte's own prior content is ignored, alive-counter acceptance rule (first-ever, sequential, duplicate, skipped, wraparound) |
| `test_com_pack` | **Done** — COM Tx produces exact golden-vector bytes, alive counter advances only after a successful PduR_ComTransmit, cyclic period gating, nothing sent before group start, out-of-range Com_SendSignal rejected, raw CAN injection reaches a mock RTE callback, wrong DLC/CRC/counter/signal-range each invalidate the whole Rx PDU without partial updates, Rx timeout supervision; uses fake PduR/Gpt |
| `test_pdur` | **Done** — ComTransmit forwards to CanIf_Transmit (payload and downstream failure both forwarded), unmapped id rejected, CanIfRxIndication forwards to Com_RxIndication, CanIfTxConfirmation forwards to Com_TxConfirmation via reverse route lookup, unmapped ids rejected on both, duplicate route source rejected at Init, calls before Init rejected; uses fake CanIf/Com |
| `test_canif` | **Done** — Tx accepted path, wrong DLC, unmapped/out-of-range Tx PDU id, controller not started, TxConfirmation forwards to PduR, Rx accepted path forwards to PduR, unmapped CAN ID, wrong DLC, duplicate Rx CAN ID rejected at Init, calls before Init rejected; uses fake Can and fake PduR |
| `test_can` | **Done** (not separately named in section 14.1's list, added per Phase 3's "loopback test on vcan" exit criterion) — transmit observed on the real vcan0 bus, oversized write rejected, controller-not-started rejects write, frames dropped while stopped are not indicated then resume once started, unopenable interface blocks start (REQ-ECUM-001), invalid write params rejected; uses real vcan0 with a fake CanIf |
| `test_rte_sensor` | **Done** — signal writes map onto COM (REQ-RTE-004), write failure forwarded, service call returns value+validity, hardware-invalidity propagation, REQ-IOHWAB-003 zero-on-invalid (not last-good), speed clamping, never-received before first reception, timeout/integrity status transitions, null pointers rejected, calls before Rte_Start rejected; uses fake Com/IoHwAb |
| `test_rte_cluster` | **Done** — signal writes map onto COM, atomic multi-element Rx update (REQ-RTE-003), an integrity-invalid frame does not overwrite the previous value (REQ-ASW-005), never-received then timeout, display port reaches IoHwAb, one task activation drives the SW-C exactly once, null pointers and pre-start calls rejected; uses fake Com/IoHwAb |
| `test_cluster_swc` | **Done** — every section 4.4 transition, three-consecutive-frame recovery to each of OFF/NORMAL/OVERSPEED depending on data, overspeed hysteresis at the exact 9700/10000 raw thresholds, communication invalidity (timeout/invalid/never-received) outranks ignition and speed, displayed-speed clamping to full scale, overspeed flag tracks state, one activation writes all three port elements exactly once, a display write failure never changes the state machine; uses a fake RTE |
| `test_virtual_hw` | **Done** — bounded drain, latest wins, malformed/oversized rejected, duplicate/reordered SEQ rejected, send with no peer non-fatal; uses real sockets |
| `test_adc_dio` | **Done** — raw pass-through without scaling, staleness age reporting, unknown channel, default level before first datagram; uses fake VirtualHw/Gpt |
| `test_iohwab` | **Done** — conversion vectors at 0/2048/4095 counts (truncation, not rounding, verified explicitly), out-of-range counts invalid + DEM_EVENT_SPEED_SENSOR_RANGE failed, stale/never-sampled input invalid + DEM_EVENT_SENSOR_LINK_LOST failed (aggregated per activation across both signals, not per signal), ignition level mapping including staleness, display frame assembled verbatim per section 15.5.2, state-name/clamping cases, display write failure counted and non-fatal, null pointers and post-DeInit calls rejected; uses fake Adc/Dio/Display/Gpt and the real Dem |
| `test_display` | **Done** — frame transported verbatim, write failure counted and reported, minimum write period respected, oversized frame rejected; uses fake VirtualHw/Gpt |
| `test_scheduler` | **Done** — due ordering, missed periods, monotonic deadline advancement; deterministic via a fake SysTimer (switched from a fake Gpt in the SysTimer/Os_Counter split — the scheduler now reads only Os_Counter, never Gpt) |
| `test_det_dem` | **Done** — bounded counters/ring buffer, failed/passed event transitions |
| `test_systimer` | **Done** (not in the original section 14.1 list; added by the SysTimer/Os_Counter split) — read before init rejected, ticks monotonic, ticks advance with real time, null/inconsistent config reported but the time base still starts, read after DeInit rejected; uses the real driver against the real host clock (like test_virtual_hw/test_can, this is the module that owns the clock, so a double would test nothing) |
| `test_os_counter` | **Done** (added by the same split) — counter zeroed at init against the current tick (not against zero), follows the timer through single steps/jumps/relative advances, tick-to-millisecond conversion including the max tick value, conversion never decreases, read before init rejected, correct across the 32-bit tick rollover; uses the real counter against a fake SysTimer |
| `test_config` | **Done** — the actual shipping SensorEcu tables accepted with zero Det errors and proven wired end to end (a signal send actually reaches the config); duplicate Rx CAN ID, Tx-PDU-index-mismatch, duplicate route source, a valid-but-incomplete route table rejected only at use (not at Init) with a Det record, overlapping signal bit ranges, a signal placed on the alive-counter/CRC field, wrong I-PDU length, and Os task-table validation (zero period, null callback, null table) |
| `test_stub_mcal` | **Done** (not in the original section 14.1 list; added for the Phase 8 `REQ-ARCH-012` proof) — the REAL `Adc.c`/`Dio.c`/`CanIf.c`/`VirtualHw_Parse.c`, completely unmodified, linked against the STUB `Can_Stub.c`/`VirtualHw_Stub.c`: injected-datagram round trip, malformed/duplicate-SEQ rejection matching the real driver, `Adc`/`Dio` reads working unmodified, `VirtualHw_Send` recording for inspection, and a `Can_Stub` loopback frame reaching the real `CanIf.c` end to end |
| `test_smoke_phase0` | **Done** — harness smoke test only, see `docs/IMPLEMENTATION_STATUS.md` |

### Phase 9 unit tests (v2.0, spec section 16)

Added for the `BswM`/`ComM`/`CanSM` split and the S32K144 hardware port.
**Happy-path only** — see `docs/REQUIREMENTS_TRACEABILITY.md`'s Phase 9
section for the specific `REQ-*` failure modes these do not yet cover
(driver DeInit completeness, failed-partial-init cleanup, the `Os`/`BswM`
layering rule, and the S32K144-variant build not pulling in POSIX test
targets — none of those has a test at all yet, happy-path or otherwise).

| Test target | Status |
|---|---|
| `test_bswm` | **Partial** — null-config rejection, initial/post-init/post-deinit state values; does **not** call `BswM_ActionList_Shutdown` and assert driver DeInit (`REQ-BSWM-001`), and does not drive a failure after partial init to check `BswM_DeInit`'s cleanup behavior (`REQ-BSWM-002`) |
| `test_comm` | **Done** for `ComM`'s own scope — null-config rejection, initial channel mode, multi-user reference counting into/out of `COMM_FULL_COMMUNICATION`, `CanSM`/`Com` side-effect calls on transition; uses fake CanSM/Com |
| `test_cansm` | **Done** for `CanSM`'s own scope — null-config rejection, state transitions `CANSM_UNINIT`→`CANSM_NOCOM`→`CANSM_FULLCOM`, `CanIf_SetControllerMode` called with the matching `CAN_CS_STARTED`/`CAN_CS_STOPPED`, `CanSM_GetCurrentComMode`; uses fake CanIf. Does not itself assert the "only `CanSM` calls `CanIf`'s controller-mode API" constraint (`REQ-CANSM-001`) — that needs a repository-wide check, not a unit test |
| `test_os_lifecycle` | **Partial** — `StartOS`-before-`Os_Init` Det rejection, `Os_Init` success, `ShutdownOS` firing the shutdown hook with the right error code; does **not** check for the absence of a `BswM` reference from `bsw/os/` (`REQ-ARCH-015`) — that is a static grep, not covered here |

No dedicated unit test exists yet for the S32K144 MCAL drivers
(`Mcu_S32K144.c`, `Can_S32K144.c`, `Adc_S32K144.c`, `Dio_S32K144.c`,
`Gpt_S32K144.c`, `Port_S32K144.c`, `SysTimer_S32K144.c`) — they require
either real hardware or a register-model fixture, neither of which exists
in this repository yet. Not started.

## Integration tests (spec section 14.2)

Automated under `tests/integration/`, driving the real `sensor_ecu`/
`cluster_ecu` binaries as subprocesses through real `AF_UNIX` links, the
real `vcan0`, and (IT-03) hand-built raw CAN frames
(`tests/integration/it_common.py` is the shared harness, including an
independent CRC-8/AUTOSAR re-implementation verified against all three of
section 6.5's golden vectors). Run via `ctest -L integration`; excluded
from a bare `ctest` since each takes several real seconds. All seven are
now implemented. See `docs/IMPLEMENTATION_STATUS.md`'s Phase 6 and Phase 8
sections for what each one actually asserts, including spec numbers
relaxed with a documented margin (IT-02's 120 ms, IT-07's 10 s) and real
bugs each test run surfaced (`Logger_Log` never flushing `stdout`; none of
the four COM-layer DEM events were ever actually being set).

| ID | Title | Status |
|---|---|---|
| IT-01 | Normal end-to-end communication | **Done** — `test_it01_normal_e2e.py` |
| IT-02 | Timeout and recovery | **Done** — `test_it02_timeout_recovery.py` |
| IT-03 | Integrity rejection | **Done** — `test_it03_integrity_rejection.py` |
| IT-04 | Layered Tx/Rx evidence | **Done** — `test_it04_layered_evidence.py` |
| IT-05 | Virtual hardware input, end to end | **Done** — `test_it05_virtual_hw_input.py` |
| IT-06 | Sensor line disconnected and restored | **Done** — `test_it06_sensor_disconnect.py` |
| IT-07 | Display link absent | **Done** — `test_it07_display_absent.py` |

## Static checks (spec section 14.3)

| Check | Status |
|---|---|
| GCC build | **Done through Phase 8 code** — GCC 9.4.0 builds cleanly with all mandatory warnings. (Clang was dropped from the routine build/test cycle per user direction after Phase 4 — see `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md`'s scope; not re-verified since.) |
| `clang-format --dry-run --Werror` | **Done through Phase 8 code** — passes on every `.c`/`.h` after `clang-format -i` fixups. |
| Sanitizer integration test | **Done** — a separate build tree with `-fsanitize=address,undefined -fno-omit-frame-pointer` builds cleanly; all 19 unit tests and all 7 integration tests pass with zero sanitizer diagnostics (checked by grepping every ECU log, not just exit codes). Found and fixed one real, reproducible issue: `Com.c`'s bit-pack/unpack helpers pass `-Wsign-conversion` under the normal build but fail it under `-fsanitize=undefined` with identical GCC/flags otherwise — see `docs/IMPLEMENTATION_STATUS.md`, Phase 8. Not yet wired as an automated CI step. |
| SW-C forbidden-header check | **Manually verified through Phase 5** — grep confirms no file under `asw/` includes a BSW/MCAL header or calls `IoHwAb_`/`Adc_`/`Dio_`/`Display_`; not yet wired as an automated CI step |
| `REQ-ARCH-009` MCAL isolation grep | **Manually verified through Phase 8** — `clock_gettime`/`sys/socket.h`/`sys/un.h`/`linux/can.h`/`linux/can/raw.h`/`AF_UNIX`/`AF_CAN`/`PF_CAN` all appear only in `bsw/mcal/` or the exempt `tests/`; the one documented exception is the default socket-path literal in each `ecu/<Ecu>/main.c` (section 15.4 requires the argument default to exist somewhere); not yet wired as an automated CI step |
| `REQ-ARCH-014` MCAL/OS timer isolation grep | **Manually verified through Phase 4.5** — no file under `bsw/os/` contains the string `Gpt_`; not yet wired as an automated CI step |
| `REQ-ARCH-015` `Os`/`BswM` isolation grep (v2.0) | **FAILS as of Phase 9** — `grep -rn "BswM" bsw/os/` finds `bsw/os/Os/Os.c`'s `StartOS` calling `BswM_GetState()`; see `docs/IMPLEMENTATION_STATUS.md` Phase 9 |
| `REQ-ARCH-016` S32K144-variant build excludes host tests (v2.0) | **FAILS as of Phase 9** — `CMakeLists.txt` still adds every `test_*`/`sensor_ecu`/`cluster_ecu` unconditionally; a `-DAUTOSAR_MCAL_VARIANT=s32k144` configure has not been proven to build cleanly |
| `REQ-ARCH-012` stub-MCAL build | **Done** — new CMake cache variable `AUTOSAR_MCAL_VARIANT` (`real`/`stub`) swaps only `Can.c`/`VirtualHw.c` for in-memory-buffer equivalents; `test_stub_mcal` (always built) proves the substitution seam; a full `-DAUTOSAR_MCAL_VARIANT=stub` build produces both working ECU executables with zero changes outside `bsw/mcal/`. See `docs/IMPLEMENTATION_STATUS.md`, Phase 8, for the verification log and one documented (non-bug) side effect of the stub's per-process loopback design. |
| `malloc`/`calloc`/`realloc`/`free` search | **Manually verified through Phase 8** — none found; not yet wired as an automated CI step |
| Python static check (`pyflakes`) | **Manually verified through Phase 8** — clean on all of `tools/hmi/*.py` and `tests/integration/*.py`; not required by the spec, done anyway since the tooling is real, runnable Python |

## How to run what exists today

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
bash tools/setup_vcan.sh                      # needed by test_can and every integration test
ctest --test-dir build --output-on-failure -LE integration   # fast: unit tests only
ctest --test-dir build --output-on-failure -L integration    # slower: real subprocess tests
```

Sanitizer run:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure -LE integration
IT_BUILD_DIR=build-asan ctest --test-dir build-asan --output-on-failure -L integration
```

`REQ-ARCH-012` stub-MCAL build:

```bash
cmake -S . -B build-stubmcal -DCMAKE_BUILD_TYPE=Debug -DAUTOSAR_MCAL_VARIANT=stub
cmake --build build-stubmcal --parallel
./build-stubmcal/sensor_ecu --duration 2   # runs its full lifecycle with no vcan0/sockets needed
```
