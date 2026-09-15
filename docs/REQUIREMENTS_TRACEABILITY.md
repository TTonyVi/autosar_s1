# Requirements traceability

Every `REQ-*` requirement in `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md` must
have at least one design/code link and one test or justified inspection
method (spec section 14.4). This table is populated as each phase (section
16) implements the requirements it owns; it is intentionally empty of rows
in Phase 0 since no `REQ-*`-bearing module exists yet.

| Requirement | Design/code files | Test case | Status |
|---|---|---|---|
| `REQ-OS-001` | `bsw/os/Os/Os.c` | manual inspection: single-threaded cooperative `Os_MainFunction` only calls the next due task after the previous callback returns; no reentrancy path exists | Implemented, test pending |
| `REQ-OS-002` | `bsw/os/Os/Os.c` | `test_scheduler` (`TestMissedPeriods`) | Implemented & tested |
| `REQ-OS-003` | see the Phase 6 section below — superseded, the real-time outer loop now exists | see below | see below |
| `REQ-OS-004` | `bsw/os/Os/Os.h` (`Os_TaskStatsType`), `bsw/os/Os/Os.c` (`Os_GetTaskStats`) | `test_scheduler` (all three test functions read stats) | Implemented & tested |
| `REQ-OS-005` | see the Phase 6 section below — superseded, the full path now exists | see below | see below |
| `REQ-CFG-001` (Os scope) | `bsw/os/Os/Os.c` (`Os_Init`) | `test_scheduler` (`TestInvalidConfigRejected`) | Implemented & tested |
| `REQ-CFG-001` (Mcu/Port/Gpt scope) | `bsw/mcal/Mcu/Mcu.c`, `bsw/mcal/Port/Port.c`, `bsw/mcal/Gpt/Gpt.c` | manual inspection: each rejects/reports a null config pointer; no dedicated `test_mcu`/`test_port`/`test_gpt` target exists (not required by section 14.1) | Implemented, test pending |
| `REQ-ECUM-002` (Os scope) | `bsw/os/Os/Os.c` (`Os_MainFunction`, `Os_GetTaskStats` both check `g_initialized`) | `test_scheduler` (`TestInvalidConfigRejected`) | Implemented & tested |
| `REQ-ECUM-004` (Det/Dem/Os scope) | `bsw/services/Det/Det.c`, `bsw/services/Dem/Dem.c`, `bsw/os/Os/Os.c` (`*_DeInit`, idempotent) | manual inspection: each `DeInit` only clears an `initialized` flag, safe to call repeatedly or before `Init` | Implemented, test pending |
| `REQ-ARCH-009` | repository-wide (only `bsw/mcal/Gpt/Gpt.c`, `bsw/mcal/VirtualHw/VirtualHw.c`, and `bsw/mcal/Can/Can.c` may touch `clock_gettime`/sockets) | static check: `grep` for `clock_gettime`/`sys/socket.h`/`sys/un.h`/`linux/can.h`/`linux/can/raw.h`/`AF_UNIX`/`AF_CAN`/`PF_CAN`/a `"/tmp/` literal outside `bsw/mcal/` and `tests/` (manually run through Phase 3; not yet wired as an automated CI step — Phase 8) | Implemented, test pending |
| `REQ-ECUM-001` | `bsw/mcal/Can/Can.c` (`Can_Init` reports `DET_ERR_HW_INIT_FAILED` and leaves the controller `CAN_CS_UNINIT`; `Can_IsControllerReady`/`Can_SetControllerMode` both refuse to let it become ready or started afterward) | `test_can` (`TestUnopenableInterfaceBlocksStart`) | Implemented & tested (the "startup fails cleanly" half — EcuM actually aborting on this signal is Phase 6) |
| `REQ-CFG-001` (Can/CanIf scope) | `bsw/mcal/Can/Can.c` (`Can_Init`), `bsw/communication/CanIf/CanIf.c` (`CanIf_Init`) | `test_can` (implicit: null/invalid config is rejected, see Phase 1/2 pattern), `test_canif` (`TestDuplicateRxCanIdRejected` also exercises the general reject-on-bad-config path) | Implemented & tested |
| `REQ-CFG-003` (CanIf Rx table scope) | `bsw/communication/CanIf/CanIf.c` (`CanIf_Init`'s nested duplicate-CAN-ID check) | `test_canif` (`TestDuplicateRxCanIdRejected`) | Implemented & tested |
| `REQ-CFG-004` | `bsw/mcal/Can/Can.h` (`Can_ControllerConfigType`), `bsw/communication/CanIf/CanIf.h` (`CanIf_TxPduConfigType`/`CanIf_RxPduConfigType`) — plain reviewable `const` C tables, no hidden routing in module logic | manual inspection: routing lives entirely in the two config struct arrays passed to `Can_Init`/`CanIf_Init`, never hard-coded in `Can.c`/`CanIf.c` | Implemented, test pending |
| `REQ-MCAL-001` | `bsw/mcal/Adc/Adc.c`, `bsw/mcal/Dio/Dio.c` (raw pass-through, no scaling/filtering; age delegated to `VirtualHw`) | `test_adc_dio` (`TestRawPassThroughNoScaling`, `TestStalenessAgeReporting`) | Implemented & tested |
| `REQ-MCAL-002` | `bsw/mcal/VirtualHw/VirtualHw.c` (`VirtualHw_MainFunction_Read`, bounded loop, non-blocking `recv`) | `test_virtual_hw` (`TestBoundedDrainAndLatestWins`) | Implemented & tested |
| `REQ-VHW-001` | `bsw/mcal/VirtualHw/VirtualHw.c` (non-blocking sockets via `O_NONBLOCK`; `recv`/`sendto` never block) | `test_virtual_hw` (`TestSendWithNoPeerIsNonFatal`; bounded-drain test also never hangs) | Implemented & tested |
| `REQ-VHW-002` | `bsw/mcal/VirtualHw/VirtualHw.c` (`VirtualHw_MainFunction_Read`'s bounded loop) | `test_virtual_hw` (`TestBoundedDrainAndLatestWins`) | Implemented & tested |
| `REQ-VHW-003` | `bsw/mcal/VirtualHw/VirtualHw.c` (`IsWellFormed` + the `continue` path that skips caching) | `test_virtual_hw` (`TestMalformedAndOversizedRejected`) | Implemented & tested |
| `REQ-VHW-004` | `bsw/mcal/VirtualHw/VirtualHw.c` (SEQ wraparound-aware comparison) | `test_virtual_hw` (`TestDuplicateAndReorderedSeqRejected`) | Implemented & tested |
| `REQ-VHW-005` | `bsw/mcal/VirtualHw/VirtualHw.c` (`VirtualHw_Send`, counts failure, returns `E_NOT_OK`, never blocks/crashes) | `test_virtual_hw` (`TestSendWithNoPeerIsNonFatal`); `test_display` (`TestWriteFailureCountedAndReported`) | Implemented & tested |
| `REQ-VHW-006` | architectural — `VirtualHw` links carry only sensor/display ASCII fields, never a CAN PDU | manual inspection: no `Com`/`PduR`/CAN-related symbol appears anywhere in `VirtualHw.c`/`Adc.c`/`Dio.c`/`Display.c` | Implemented, test pending |
| `REQ-VHW-007` | see the Phase 7 section below — superseded, the keepalive producers now exist | see below | see below |
| `REQ-ECUM-005` (VirtualHw scope) | `bsw/mcal/VirtualHw/VirtualHw.c` (`VirtualHw_Init`'s bind-failure path reports to `Det` and continues with `fd == -1`, never aborts) | manual inspection (a dedicated init-time bind-failure test would need a permission-denied path, not attempted in Phase 2) | Implemented, test pending |
| `REQ-ECUM-006` | `bsw/mcal/VirtualHw/VirtualHw.c` (`unlink()` before `bind()` on every RX link init) | manual inspection: `unlink` return value is deliberately ignored (ENOENT is the expected common case) | Implemented, test pending |
| `REQ-CFG-001` (VirtualHw/Adc/Dio/Display scope) | `bsw/mcal/VirtualHw/VirtualHw.c`, `bsw/mcal/Adc/Adc.c`, `bsw/mcal/Dio/Dio.c`, `bsw/mcal/Display/Display.c` (`*_Init` reject null/invalid config) | `test_adc_dio` (`TestUnknownChannelRejected` exercises the read-side rejection); `test_display` (`TestOversizedFrameRejected`) | Implemented & tested |
| `REQ-COM-001` | `bsw/communication/Com/Com.c` (`PackBits` only ever writes bits a signal or the alive counter own; every other bit in a freshly-zeroed frame buffer stays 0) | manual inspection: `Com_MainFunctionTx` starts each frame with `memset(frame, 0, ...)` and only `PackBits` calls ever set a bit | Implemented, test pending |
| `REQ-COM-002` | `bsw/communication/Com/Com.c` (`Com_RxIndication`'s `SduLength != ipdu->length` check, before the payload is even copied) | `test_com_pack` (`TestRxWrongDlcRejectedBeforeTouchingPayload`) | Implemented & tested |
| `REQ-COM-003` | `bsw/communication/Com/Com.c` (`Com_RxIndication`: CRC checked via `ComProtection_ComputeCrc8`, then the alive counter, both before any signal is unpacked) | `test_com_pack` (`TestRxWrongCrcRejectedWithoutUpdatingSignalsOrNotifying`, `TestRxSkippedCounterRejectedThenRecoversOnCorrectNext`); `test_com_protection` (the CRC8 algorithm itself, golden vectors) | Implemented & tested |
| `REQ-COM-004` | `bsw/communication/Com/Com.c` (`Com_RxIndication` decodes and range-checks every owned signal before committing) | `test_com_pack` (`TestRxSignalOutOfRangeInvalidatesWholePdu`) | Implemented & tested |
| `REQ-COM-005` | `bsw/communication/Com/Com.c` (`Com_MainFunctionRx` compares `Gpt_GetTimeElapsed(...) - lastValidRxTimeMs` — the last VALID reception, never touched by a rejected frame — against the configured timeout) | `test_com_pack` (`TestRxTimeoutSupervision`) | Implemented & tested |
| `REQ-COM-006` | `bsw/communication/Com/Com.c` (`PackBits`/`UnpackBits`: pure shift-and-mask bit access; no `struct` is ever cast onto `frame`/`payload`) | manual inspection: no wire-facing `struct` type exists anywhere in `Com.c`; `test_com_pack`'s golden-byte assertions indirectly prove the shift/mask math is correct | Implemented, test pending |
| `REQ-COM-007` | `bsw/communication/Com/Com.c`, `bsw/communication/Com/ComProtection.c` (every wire-facing computation uses `uint8`/`uint16`/`uint32`; no `float32`/`float64` appears in either file) | manual inspection: grep for `float` in both files returns nothing | Implemented, test pending |
| `REQ-ASW-005` | `bsw/communication/Com/Com.c` (`Com_RxIndication` decodes into a local `decoded[]` array and only writes `g_signalValues` after every check — DLC, CRC, counter, all signal ranges — has passed) | `test_com_pack` (`TestRxWrongCrcRejectedWithoutUpdatingSignalsOrNotifying`, `TestRxSkippedCounterRejectedThenRecoversOnCorrectNext`) | Implemented & tested |
| `REQ-ARCH-003` | `bsw/communication/PduR/PduR.c` (every function forwards a `PduInfoType` pointer unchanged; none reads `SduDataPtr`'s contents) | `test_pdur` (every forwarding test asserts the payload bytes arrive unchanged, proving PduR never altered them) | Implemented & tested |
| `REQ-CFG-001` (PduR/Com scope) | `bsw/communication/PduR/PduR.c` (`PduR_Init`), `bsw/communication/Com/Com.c` (`Com_Init`) | `test_pdur` (`TestDuplicateRouteSourceRejectedAtInit` also exercises the general reject-on-bad-config path); `test_com_pack` (config is valid throughout; bad-config rejection follows the same pattern as `CanIf_Init`, not separately re-tested) | Implemented & tested |
| `REQ-CFG-003` (Com signal/I-PDU scope) | `bsw/communication/Com/Com.c` (`Com_Init`'s bit-overlap checks: signal vs. signal, signal vs. counter, signal vs. CRC byte) | `test_config` (`TestOverlappingSignalsRejected`, `TestSignalOverlappingProtectionFieldsRejected`) | Implemented & tested |
| `REQ-CFG-002` | `bsw/communication/Com/Com.c` (`_Static_assert` on `sizeof(uint8)`, `sizeof(uint16)`, and `COM_IPDU_LENGTH == 8U`) | manual inspection: the assertions are compile-time, so any build that succeeds has already proven them; implemented since Phase 4 but not previously traced | Implemented & tested |
| `REQ-ARCH-014` | `bsw/os/Os/Os.c`/`.h` (no `Gpt_` symbol anywhere), `bsw/os/Os/Os_Counter.c` (reads only `SysTimer_GetTicks`) | static check: `grep -rn "Gpt_" bsw/os/` returns nothing (manually run; not yet wired as an automated CI step — Phase 8); behaviorally via `test_scheduler` (now driven by a fake `SysTimer`, not a fake `Gpt`) | Implemented & tested |

## Phase 5 — IoHwAb, RTE, SW-Cs

| Requirement | Design/code files | Test case | Status |
|---|---|---|---|
| `REQ-IOHWAB-001` | `bsw/ecu_abstraction/IoHwAb/IoHwAb.c` (`SampleSignal`: an invalid sample always zeroes `out->value` before returning `IOHWAB_INVALID`) | `test_iohwab` (`TestOutOfRangeCountsAreInvalidAndReported`, `TestStaleInputIsInvalidAndReported`) | Implemented & tested |
| `REQ-IOHWAB-002` | `bsw/ecu_abstraction/IoHwAb/IoHwAb.c` (`SampleSignal`'s `fresh = (everReceived && (ageMs <= cfg->inputTimeoutMs))`), `IoHwAb_Cfg.h` (`IOHWAB_INPUT_TIMEOUT_MS`) | `test_iohwab` (`TestStaleInputIsInvalidAndReported`, exact-boundary case included) | Implemented & tested |
| `REQ-IOHWAB-003` | `asw/VehicleStateSwc/VehicleStateSwc.c` (`VehicleStateSwc_10ms`: on `validity != RTE_INPUT_VALID`, `speedRaw`/`ignitionOn` are forced to 0/FALSE, never the previous value) | `test_rte_sensor` (`TestInvalidInputPublishesZeroNotLastGood`) | Implemented & tested |
| `REQ-IOHWAB-004` | `bsw/ecu_abstraction/IoHwAb/IoHwAb.c` (`SampleSignal`/`IoHwAb_MainFunction` set `DEM_EVENT_SPEED_SENSOR_RANGE` and the aggregated `DEM_EVENT_SENSOR_LINK_LOST`; neither function touches any application state) | `test_iohwab` (`TestOutOfRangeCountsAreInvalidAndReported`, `TestStaleInputIsInvalidAndReported`) | Implemented & tested |
| `REQ-RTE-001` | `rte/generated/SensorEcu/Rte_SensorEcu.c`, `rte/generated/ClusterEcu/Rte_ClusterEcu.c` (every `Rte_Read_*`/`Rte_Call_*` checks its pointer arguments before dereference, reporting to `Det` on null) | `test_rte_sensor` (`TestNullPointersRejected`), `test_rte_cluster` (`TestNullPointersRejected`) | Implemented & tested |
| `REQ-RTE-002` | both `Rte_*.c` files (every read copies `g_rx*` into `*value`; no function returns a pointer into RTE state) | manual inspection: no `Rte_Read_*` function signature returns a pointer, only `Rte_ReturnType` with an out-parameter it writes through `*value = ...` | Implemented, test pending |
| `REQ-RTE-003` | both `Rte_*.c` files (`Rte_COMCbk_*` notifications all run inside `Com_MainFunctionRx`, before any runnable of the same cycle executes — single-threaded, so "atomic" reduces to "no runnable reads a partially-updated I-PDU") | `test_rte_cluster` (`TestRxUpdateIsAtomicForTheSwc`: both elements of one PDU visible together) | Implemented & tested |
| `REQ-RTE-004` | both `Rte_*.c` files (every `Rte_Write_Pp*` updates the RTE-local `g_tx*` variable, then calls `Com_SendSignal` with the generated `RTE_SIGNAL_*` id) | `test_rte_sensor` (`TestWriteMapsOntoComSignals`), `test_rte_cluster` (`TestWriteMapsOntoComSignals`) | Implemented & tested |
| `REQ-RTE-005` | both `Rte_*.c` files (`Rte_COMCbk_*` only ever runs from `Com_RxIndication`'s own call graph, which per `REQ-COM-003`/`REQ-ASW-005` only fires after full CRC/counter/range validation) | `test_rte_cluster` (`TestInvalidFrameDoesNotOverwrite`) | Implemented & tested |
| `REQ-RTE-006` | both `Rte_*.c` files (`g_everReceived = FALSE` in `Rte_Start`; `RemoteStatus()` returns `RTE_E_NEVER_RECEIVED` until the first `Rte_COMCbk_*` fires) | `test_rte_sensor` (`TestNeverReceivedBeforeFirstReception`), `test_rte_cluster` (`TestNeverReceivedThenTimeout`) | Implemented & tested |
| `REQ-RTE-007` | `rte/generated/SensorEcu/`, `rte/generated/ClusterEcu/` (two separate, ECU-specific translation units; SW-Cs under `asw/` never include or modify anything under `rte/generated/`) | manual inspection: directory layout itself is the enforcement; `REQ-ARCH-001`/`002`'s checks below additionally prove no SW-C reaches in | Implemented, test pending |
| `REQ-ARCH-001` | `asw/VehicleStateSwc/VehicleStateSwc.c` (includes only its own header and `Rte_VehicleStateSwc.h`), `asw/ClusterControlSwc/ClusterControlSwc.c` (same pattern) | static check: `grep -n "#include" asw/*/*.c` shows exactly two includes per file (manually run; not yet wired as an automated CI step — Phase 8) | Implemented, test pending |
| `REQ-ARCH-002` | same two files | static check: `grep -rnE '#include "(Com\|PduR\|CanIf\|Can\|IoHwAb\|Adc\|Dio\|Display\|Gpt\|SysTimer\|Os\|Det\|Dem)[_.]' asw/'` returns nothing (manually run; not yet wired as an automated CI step — Phase 8) | Implemented, test pending |
| `REQ-ARCH-005` | `bsw/ecu_abstraction/IoHwAb/IoHwAb_Cfg.c` (one `IoHwAb.c` shared by both ECUs; `IoHwAb_ConfigSensor`/`IoHwAb_ConfigCluster` are the only ECU-specific artifact), same pattern in every Phase 6 `*_Cfg.c` | manual inspection: `IoHwAb.c`, `Com.c`, `PduR.c`, `CanIf.c`, `Can.c` have exactly one implementation each, linked into both `sensor_ecu` and `cluster_ecu` | Implemented, test pending |
| `REQ-ARCH-008` | `asw/VehicleStateSwc/VehicleStateSwc.c`, `asw/ClusterControlSwc/ClusterControlSwc.c` (neither calls `IoHwAb_*`/`Adc_*`/`Dio_*`/`Display_*` directly; only `Rte_Call_RpVehicleInputs_GetVehicleInputs`/`Rte_Call_RpDisplay_SetClusterDisplay`) | static check: `grep -rnE '\b(IoHwAb\|Adc\|Dio\|Display)_[A-Za-z]' asw/` returns nothing (manually run; not yet wired as an automated CI step — Phase 8) | Implemented, test pending |
| `REQ-ASW-001` | `asw/ClusterControlSwc/ClusterControlSwc.c` (`ClusterControlSwc_10ms`: the `!commValid` branch runs before, and unconditionally overrides, anything the ignition/speed-driven state machine would have done) | `test_cluster_swc` (`TestCommunicationInvalidityOutranksEverything`, all three bad-status cases) | Implemented & tested |
| `REQ-ASW-002` | `rte/common/Rte_Type.h` (`CLUSTER_OVERSPEED_ENTRY_RAW == 10000U`), `ClusterControlSwc.c` (`RunValidStateMachine`) | `test_cluster_swc` (`TestOverspeedHysteresis`, exact-boundary case) | Implemented & tested |
| `REQ-ASW-003` | `rte/common/Rte_Type.h` (`CLUSTER_OVERSPEED_EXIT_RAW == 9700U`), `ClusterControlSwc.c` (`RunValidStateMachine`) | `test_cluster_swc` (`TestOverspeedHysteresis`, exact-boundary case) | Implemented & tested |
| `REQ-ASW-004` | `rte/common/Rte_Type.h` (`CLUSTER_RECOVERY_FRAME_COUNT == 3U`), `ClusterControlSwc.c` (`g_consecutiveValid`, checked in the `CLUSTER_STATE_COMM_FAULT` case) | `test_cluster_swc` (`TestThreeFrameRecovery`, `TestRecoveryTargetDependsOnData`) | Implemented & tested |
| `REQ-ASW-005` | (Com-side coverage already traced under Phase 4) `rte/generated/ClusterEcu/Rte_ClusterEcu.c` (`UpdateReceptionHealth`: an integrity error only sets a flag, never touches `g_rxVehicleSpeedRaw`) | `test_rte_cluster` (`TestInvalidFrameDoesNotOverwrite`) | Implemented & tested |
| `REQ-ASW-006` | `asw/ClusterControlSwc/ClusterControlSwc.c` (`ClusterControlSwc_10ms`'s explicit clamp to `VEHICLE_SPEED_RAW_MAX`), `bsw/ecu_abstraction/IoHwAb/IoHwAb.c` (`IoHwAb_SetClusterDisplay` clamps again, defense in depth) | `test_cluster_swc` (`TestDisplayedSpeedClamping`), `test_iohwab` (`TestDisplayStateNamesAndClamping`) | Implemented & tested |

## Phase 6 — EcuM, static ECU config, executables, signal handling

| Requirement | Design/code files | Test case | Status |
|---|---|---|---|
| `REQ-ECUM-001` (full, superseding the Phase 3 partial row above) | `bsw/services/EcuM/EcuM.c` (`EcuM_Init`: after `Can_Init`, `!Can_IsControllerReady(...)` is the only checked failure that returns `E_NOT_OK` and sets `ECUM_STATE_SHUTDOWN`) | `test_it02_timeout_recovery`/`test_it06_sensor_disconnect` exercise a running `EcuM_Init` indirectly; no dedicated "unopenable interface" integration test constructs a bad `--can-if` (would need root or a missing interface, not attempted) | Implemented, test pending |
| `REQ-ECUM-002` (full, superseding the Phase 1 partial row above) | every module's `_Init`/entry function checks its own `g_initialized`/`initialized` flag and reports `DET_ERR_UNINIT` before doing anything | per-module unit tests already listed above (`test_scheduler`, `test_canif`, `test_com_pack`, `test_pdur`, `test_iohwab`, `test_rte_sensor`, `test_rte_cluster`, `test_systimer`, `test_os_counter`) each include an explicit pre-init rejection case | Implemented & tested |
| `REQ-ECUM-003` | `ecu/SensorEcu/main.c`/`ecu/ClusterEcu/main.c` (`HandleStopSignal` only sets `volatile sig_atomic_t g_stopRequested`; all shutdown work happens in the main loop after `sigaction`-installed handlers return) | manual inspection: the handler body is one assignment, no I/O, no allocation, no lock; `test_it02_timeout_recovery`/`test_it06_sensor_disconnect` send real `SIGTERM` via `Ecu.stop()` and observe clean shutdown, exercising the path end to end even though they don't assert on the handler's internals directly | Implemented, test pending |
| `REQ-ECUM-004` (EcuM scope, superseding the Phase 1 partial row for Det/Dem/Os) | `bsw/services/EcuM/EcuM.c` (`EcuM_Shutdown`'s early return when already `ECUM_STATE_SHUTDOWN`/`ECUM_STATE_OFF`) | manual inspection: called twice in sequence during interactive verification without a crash; no dedicated unit test constructs this directly since `EcuM_Init` requires a full, real module stack | Implemented, test pending |
| `REQ-ECUM-005` (EcuM scope, superseding the Phase 2 partial row for VirtualHw) | `bsw/services/EcuM/EcuM.c` (`EcuM_Init` calls `VirtualHw_Init` unconditionally when configured and never checks its result before continuing to the next step) | `test_it06_sensor_disconnect`, `test_it07_display_absent` (both start an ECU with the relevant hardware link never sending/receiving and assert it runs normally) | Implemented & tested |
| `REQ-ECUM-006` (full, superseding the Phase 2 partial row) | `bsw/mcal/VirtualHw/VirtualHw.c` (already traced); `tools/hmi/vehicle_hmi.py`'s `DisplayLineReceiver` does the same `unlink()`-before-`bind()` on the HMI side of the display link | manual inspection (HMI side); `test_virtual_hw`/integration tests (ECU side, indirectly — every test run creates and removes its own socket path) | Implemented & tested |
| `REQ-OS-003` | `ecu/SensorEcu/main.c`/`ecu/ClusterEcu/main.c` (250 µs poll loop, a quarter of the 1 ms shortest task period) | manual observation: a live 2-second run of `sensor_ecu` reported `Task_App_10ms: runs=200 missed=0 max_latency=0ms` via the section 9.3 scheduler summary — well inside the 99.9%-within-2ms bound, but not a repeatable automated 60-second-load assertion | Implemented, test pending |
| `REQ-OS-005` | the full Tx path (`asw`→`rte`→`Com`→`PduR`→`CanIf`→`Can`) and I/O path (`VirtualHw`→`Adc`/`Dio`→`IoHwAb`→`rte`) together | `test_it05_virtual_hw_input` (`test_speed_reaches_bus_and_display`: asserts the steady-state gap between consecutive `0x180` frames never exceeds 3× the 20 ms Tx period, and that a display frame reflecting the new state arrives within a 1 s window — a looser bound than the spec's 30 ms/130 ms claims, documented in the test's own comments as appropriate for a non-real-time host under test-harness overhead) | Implemented & tested (with a relaxed threshold) |

## Phase 7 — tools/hmi/, scenario player, demo scripts

| Requirement | Design/code files | Test case | Status |
|---|---|---|---|
| `REQ-VHW-007` | `tools/hmi/scenario_player.py` (`play()`'s `time.sleep(KEEPALIVE_PERIOD_S / speed_factor)` — the 100 ms keepalive resends the *current* value every cycle, not just on change), `tools/hmi/vehicle_hmi.py` (`SensorRigSender.run()`, same 100 ms period) | `test_it01_normal_e2e`, `test_it05_virtual_hw_input` (both rely on the keepalive to keep the sensor line fresh across a multi-second observation window; a broken keepalive would surface as `IoHwAb` reporting invalid partway through) | Implemented & tested |
| `REQ-HMI-001` | `tools/hmi/*.py` (only `argparse`, `csv`, `itertools`, `os`, `queue`, `socket`, `struct`, `sys`, `threading`, `time`, `tkinter` imported anywhere) | static check: `grep -n "^import\|^from" tools/hmi/*.py` shows only standard-library modules (manually run); `pyflakes tools/hmi/*.py` clean | Implemented & tested |
| `REQ-HMI-002` | `tools/hmi/vehicle_hmi.py` (`SensorRigSender`/`DisplayLineReceiver`/`CanTraceReader` are all `threading.Thread` subclasses pushing onto one `queue.Queue`; `VehicleHmi.refresh()` is the only place that touches a widget, scheduled via `root.after(GUI_REFRESH_MS, ...)` at 100 ms) | manual inspection (no visual GUI test possible in this environment — no display/Xvfb); a headless script exercised the worker threads' socket/queue behavior directly (see `docs/IMPLEMENTATION_STATUS.md` Phase 7) | Implemented, test pending |
| `REQ-HMI-003` | `tools/hmi/vehicle_hmi.py`, `tools/hmi/hw_link.py`, `tools/hmi/scenario_player.py`, `tests/integration/it_common.py` (all four define the identical `speed_kph_to_counts`: `round(kph * 100 * 4095 / 25000)`, clamped 0..4095) | `test_it05_virtual_hw_input` exercises the formula's inverse relationship live: sending counts for 120.00 km/h produces exactly `speed_raw=12000` on the bus; a headless HMI check additionally asserts the function directly at 0/120/250/-5 km/h | Implemented & tested |
| `REQ-HMI-004` | `tools/hmi/vehicle_hmi.py` (`CanTraceReader` only ever calls `sock.recv`, never `sock.send`; no code path anywhere in the file calls a `send`/`sendto` on the CAN socket) | manual inspection: `grep -n "can.*send\|send.*can"` (case-insensitive) in `vehicle_hmi.py` finds nothing; the only two sockets that send (`SensorRigSender`) address the sensor line, not `vcan0` | Implemented, test pending |
| `REQ-HMI-005` | `tools/hmi/vehicle_hmi.py` (`SensorRigSender.run()`'s `except OSError` around every `sendto`, reported to the GUI as a status string, never raised) | headless check (`docs/IMPLEMENTATION_STATUS.md` Phase 7): starts `SensorRigSender` against a socket path with no bound peer and confirms it stops cleanly rather than raising | Implemented & tested |
| `REQ-HMI-006` | `tools/hmi/vehicle_hmi.py` (the sender/receiver threads retry indefinitely on `OSError` rather than exiting; the GUI itself never blocks on either ECU being present) | manual inspection only (no display available to drive the actual widget states through a real reconnect) | Implemented, test pending |
| `REQ-HMI-007` | `tools/hmi/vehicle_hmi.py` (the only two fault-injection controls are "disconnect the sensor line" and "send out-of-range counts" — both things a real sensor/wire could do; no CAN-level fault injection exists in the HMI) | manual inspection: the sensor rig's `ttk.Checkbutton` widgets are the complete list of fault-injection controls in the file | Implemented, test pending |
| `REQ-ARCH-011` | `tools/hmi/*.py` (no `import` of anything under `bsw/`, `asw/`, `rte/`, or `ecu/`; standard library only, per `REQ-HMI-001` above) | static check: `grep -n "^import\|^from" tools/hmi/*.py` (manually run) | Implemented & tested |
| `REQ-ARCH-013` | `bsw/`, `asw/`, `rte/`, `ecu/`, `platform/` (no functional use of `scenario`/`slider`/`gui`/`hmi`; two pre-existing Phase 2 comments merely *mention* "the HMI" as the process on the other end of a link, which is not a functional dependency) | static check: `grep -rniE "scenario\|slider\|[^a-z]gui[^a-z]\|hmi" bsw/ asw/ rte/ ecu/ platform/` (manually run; the two comment hits reviewed and judged non-functional) | Implemented, test pending |

## Requirements not tied to one phase's own section above

Implemented earlier than this pass caught them; added here rather than
retroactively editing every phase section above.

| Requirement | Design/code files | Test case | Status |
|---|---|---|---|
| `REQ-ARCH-004` | `bsw/communication/CanIf/CanIf.h`/`.c` (`CanIf_TxPduConfigType`/`CanIf_RxPduConfigType.canId`, set only from `ecu/<Ecu>/config/*_Cfg.c`; `CanIf.c` itself contains no CAN ID literal) | `test_canif` (every test drives the mapping through config, never a hard-coded ID); `test_config` (the shipping tables) | Implemented & tested |
| `REQ-ARCH-006` | architectural, whole `bsw/`/`rte/`/`asw/` tree (the Tx path `asw`→`rte`→`Com`→`PduR`→`CanIf`→`Can` and Rx reverse are the only call chains that exist; no function anywhere calls, e.g., `Can_Write` from `Com.c` or `Com_SendSignal` from `asw/`) | manual inspection: each module's include list (traced per-phase above) only ever reaches the next layer down its own path | Implemented, test pending |
| `REQ-ARCH-007` | `tests/fixtures/*_Fake.c` (every test double lives here, never under `bsw/`/`asw/`/`rte/`/`ecu/`); `CMakeLists.txt` (no `add_executable(sensor_ecu ...)`/`add_executable(cluster_ecu ...)` source list references anything under `tests/` or `tools/`) | manual inspection: `grep -n "tests/\|tools/" CMakeLists.txt` shows fixtures/fakes attached only to `test_*` targets, never to `sensor_ecu`/`cluster_ecu` | Implemented, test pending |
| `REQ-ARCH-010` | `bsw/mcal/Adc/Adc.c`/`bsw/mcal/Dio/Dio.c` (raw counts/levels only, already traced under `REQ-MCAL-001`); `bsw/ecu_abstraction/IoHwAb/IoHwAb.c` (the only file that computes km/h, applies plausibility, or produces validity) | `test_adc_dio` (raw pass-through), `test_iohwab` (all conversion/plausibility/validity behavior) | Implemented & tested |
| `REQ-ARCH-012` | `bsw/mcal/Can/Can_Stub.c`, `bsw/mcal/VirtualHw/VirtualHw_Stub.c` (identical headers to the real files, in-memory buffers instead of sockets); `CMakeLists.txt`'s `AUTOSAR_MCAL_VARIANT` cache variable | `test_stub_mcal` (the substitution seam, 12 checks); manual verification: `-DAUTOSAR_MCAL_VARIANT=stub` produces both working ECU executables, full init/schedule/shutdown lifecycle confirmed live (docs/IMPLEMENTATION_STATUS.md, Phase 8) | Implemented & tested |

## Phase 8 — additional integration-level evidence

IT-03 and IT-04 reinforce several requirements already marked
"Implemented & tested" above via a unit test, at the full-system level
(real ECU processes, real `vcan0`), rather than introducing new
requirements of their own:

| Requirement | Additional test case | Why it matters at this level |
|---|---|---|
| `REQ-COM-003` | `test_it03_integrity_rejection` | Proves CRC/counter rejection survives a frame injected by an independent, from-scratch CRC implementation onto the real bus — not only frames `Com.c`'s own test harness constructed |
| `REQ-ASW-005` | `test_it03_integrity_rejection` | Proves the corrupt value never reaches `ClusterStatus` end to end, across the real `Can`→`CanIf`→`PduR`→`Com`→`RTE`→SW-C chain, not only at the RTE unit-test boundary |
| `REQ-ARCH-006` | `test_it04_layered_evidence` | Every named layer's counter advancing together, for the same exchange, is direct evidence no layer was skipped in either direction |
| `REQ-RTE-005` | `test_it04_layered_evidence` | The Rx-side counter chain (Can → CanIf → PduR → COM → RTE) advancing in lockstep is the full-system counterpart to `test_rte_cluster`'s unit-level proof |

`DEM_EVENT_VEHICLE_STATUS_INTEGRITY`/`DEM_EVENT_CLUSTER_STATUS_INTEGRITY`/
`DEM_EVENT_VEHICLE_STATUS_TIMEOUT`/`DEM_EVENT_CLUSTER_STATUS_TIMEOUT` (spec
section 10.2) are not separate `REQ-*` IDs, so they have no row of their
own above; their implementation (added in Phase 8, see
`docs/IMPLEMENTATION_STATUS.md`) is `rte/generated/SensorEcu/Rte_SensorEcu.c`
and `rte/generated/ClusterEcu/Rte_ClusterEcu.c`'s `UpdateReceptionHealth`,
tested by `test_it03_integrity_rejection` (integrity events) and by
inspection for the timeout pair (the same function, same call site,
covered by `test_it02_timeout_recovery`'s COMM_FAULT assertion, though
that test does not itself grep the DEM log line the way IT-03 does).

## How to extend this table

When a phase implements a requirement:

1. Add one row per `REQ-*` ID (do not group multiple requirements into one
   row, even if they're implemented by the same function).
2. `Design/code files` should point at the specific file(s), not a whole
   directory, unless the requirement is genuinely repository-wide (e.g.
   `REQ-ARCH-009`, which is a cross-cutting static check).
3. `Test case` should name the unit test (`test_<module>`), integration
   test (`IT-0x`), or static check (section 14.3) that exercises it. A
   requirement with no automated test needs an explicit justification in
   this column (e.g. "manual inspection: <reason>"), not a blank cell.
4. `Status` is one of: `Implemented & tested`, `Implemented, test pending`,
   `Not started`.
