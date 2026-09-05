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
| `REQ-OS-003` | — | Not applicable at Phase 1: `Os_MainFunction` has no real-time outer loop yet (added in Phase 6 with `clock_nanosleep`); this 99.9%-within-2ms requirement needs a real host clock and real load | Not started |
| `REQ-OS-004` | `bsw/os/Os/Os.h` (`Os_TaskStatsType`), `bsw/os/Os/Os.c` (`Os_GetTaskStats`) | `test_scheduler` (all three test functions read stats) | Implemented & tested |
| `REQ-OS-005` | — | Not applicable at Phase 1: needs the full communication + I/O path (Phase 6) | Not started |
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
| `REQ-VHW-007` | not yet implemented — the 100ms keepalive is a producer-side behavior; the only current producers are `tools/hmi/hw_link.py` (`--repeat`) and the future scenario player/HMI (Phase 7) | — | Not started |
| `REQ-ECUM-005` (VirtualHw scope) | `bsw/mcal/VirtualHw/VirtualHw.c` (`VirtualHw_Init`'s bind-failure path reports to `Det` and continues with `fd == -1`, never aborts) | manual inspection (a dedicated init-time bind-failure test would need a permission-denied path, not attempted in Phase 2) | Implemented, test pending |
| `REQ-ECUM-006` | `bsw/mcal/VirtualHw/VirtualHw.c` (`unlink()` before `bind()` on every RX link init) | manual inspection: `unlink` return value is deliberately ignored (ENOENT is the expected common case) | Implemented, test pending |
| `REQ-CFG-001` (VirtualHw/Adc/Dio/Display scope) | `bsw/mcal/VirtualHw/VirtualHw.c`, `bsw/mcal/Adc/Adc.c`, `bsw/mcal/Dio/Dio.c`, `bsw/mcal/Display/Display.c` (`*_Init` reject null/invalid config) | `test_adc_dio` (`TestUnknownChannelRejected` exercises the read-side rejection); `test_display` (`TestOversizedFrameRejected`) | Implemented & tested |

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
