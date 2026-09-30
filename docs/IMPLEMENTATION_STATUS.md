# Implementation status

> **v3.0 note:** this tree pivoted to S32K144-hardware-only (spec
> `AUTOSAR_CLASSIC_PROJECT_SPEC_v3.0.md`). Phases 0-9 below describe how
> the code was originally built and verified — against the Ubuntu
> simulation, which is now frozen, unchanged, in `legacy-sim/` (its own
> copy of this file has the same Phase 0-9 history and is still accurate
> for that snapshot). This copy's Phase 0-9 rows are kept as an honest
> history of how the BSW/RTE/ASW logic now running on real hardware was
> actually developed and tested — not a claim that it has been re-tested
> since. See the new **Phase 10** entry and section below for the pivot
> itself.

Tracks progress against the phase plan in `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md`
(Phases 0-9, historical) and `AUTOSAR_CLASSIC_PROJECT_SPEC_v3.0.md` (Phase
10 on). Updated at the end of every phase, per the spec's own instruction.

| Phase | Title | Status | Notes |
|---:|---|---|---|
| 0 | Repository and documentation baseline | **Done** (legacy-sim) | See below. |
| 1 | Base MCAL, services, time, and scheduler | **Done** (legacy-sim) | See below. |
| 2 | Virtual hardware links and input/output MCAL | **Done** (legacy-sim) | See below. |
| 3 | Can Driver and CanIf | **Done** (legacy-sim) | See below. |
| 4 | PduR and COM | **Done** (legacy-sim) | See below. |
| 4.5 | SysTimer / Os_Counter split | **Done** (legacy-sim) | Not a spec phase; see below. |
| 5 | IoHwAb, RTE, and SW-Cs | **Done** (legacy-sim) | See below. |
| 6 | EcuM and complete ECU integration | **Done** (legacy-sim) | See below. |
| 7 | Tkinter HMI and scenario player | **Done** (legacy-sim only — removed from this tree in Phase 10) | See below. |
| 8 | Robustness and handoff | **Done** (legacy-sim) | See below. |
| 9 | BSW mode management (`BswM`/`ComM`/`CanSM`) and S32K144 hardware port | **Delivered, known issues open** | Modules and MCAL delivered; several `REQ-*` added for this phase are still not met by the code — see below. |
| 10 | Hardware-only pivot (v3.0) | **Structurally done, unverified** | Ubuntu simulation moved to `legacy-sim/`; `Display_S32K144.c` and `ecu/ClusterEcu/main.c` added; `CMakeLists.txt` simplified. Not yet compiled with a real cross toolchain or flashed to hardware. See below. |

## Phase 0 — delivered

- Full directory skeleton per spec section 12 (`platform/`, `bsw/{services,
  communication,ecu_abstraction,mcal,os}/`, `asw/`, `rte/`, `ecu/`,
  `tests/`, `tools/hmi/`, `scenarios/`).
- Root `CMakeLists.txt` (CMake ≥3.20, C11) wiring up every target named in
  section 12.2 (`platform`, `bsw_services`, `bsw_com`, `bsw_mcal`,
  `bsw_ecuab`, `rte_sensor`, `rte_cluster`, `asw_sensor`, `asw_cluster`,
  `sensor_ecu`, `cluster_ecu`), each currently backed by a single
  placeholder translation unit, built with
  `-Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow` via
  `cmake/CompilerWarnings.cmake`.
- `platform/include/{Platform_Types.h,Std_Types.h,ComStack_Types.h}` — the
  fixed-width types and common communication-stack structs from spec
  section 7.3.
- `ecu/SensorEcu/main.c` and `ecu/ClusterEcu/main.c`: placeholder entry
  points that print an identity/version string and exit 0.
- `tools/setup_vcan.sh`: idempotent `vcan0` bring-up per section 15.1.
- `tests/unit/test_smoke_phase0.c` + a `ctest` registration, proving the
  build/test harness works end to end (real per-module unit tests start in
  Phase 1, per the coverage table in section 14.1).
- `README.md`, `docs/ARCHITECTURE.md` (layer diagram + EcuM init-order
  rationale required by section 9.2), `docs/REQUIREMENTS_TRACEABILITY.md`
  and `docs/TEST_PLAN.md` (skeletons), `docs/arxml/README.md` (full content
  per section 11.4, since it doesn't depend on later phases).
- `.clang-format` checked in per section 13.1.

### Phase 0 exit criteria (spec section 16) — verification

All verified in this dev sandbox (Ubuntu 20.04 x86_64 — **not** the
22.04/24.04 target claimed by the spec; see deviation note below):

- [x] `cmake -S . -B build && cmake --build build --parallel` succeeds with
      **both** GCC 9.4.0 and Clang 10.0.0, zero warnings, with
      `-Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow`.
- [x] `ctest` passes (1/1, `test_smoke_phase0`).
- [x] Both ECU executables start, print a placeholder version, and exit 0.
- [x] `tools/setup_vcan.sh` run twice back-to-back: first run creates and
      brings up `vcan0`, second run detects it already exists/already up
      and exits 0 both times (idempotency confirmed) —
      `ip -details link show vcan0` output printed correctly.
- [x] `clang-format --dry-run --Werror` passes on every `.c`/`.h` file
      (found real violations on first run — brace placement didn't match
      the checked-in `.clang-format`'s `BreakBeforeBraces: Attach` — fixed
      with `clang-format -i` across the tree, then re-verified clean).

#### Deviation note: verification environment vs. target platform

This sandbox is Ubuntu 20.04, whose `apt` repository only ships CMake
3.16.3, below the spec's required 3.20 (section 13.1; Ubuntu 22.04/24.04
ship 3.22/3.28, which satisfy it natively). `cmake_minimum_required` was
temporarily lowered to 3.16 to run the checks above, then restored to
`3.20` in the committed `CMakeLists.txt` before this file was written —
nothing else in the CMake code depends on the lowered version, so this is
purely an artifact of the sandbox's package availability, not a spec
change. Re-verify on an actual Ubuntu 22.04/24.04 machine before treating
Phase 0 as fully closed on the real target.

## Phase 1 — delivered

- `bsw/mcal/Gpt/{Gpt.h,Gpt.c}`: the single free-running millisecond time
  base (`GPT_CHANNEL_TIME_BASE`). **The only file in the whole project that
  calls `clock_gettime`** (section 8.1, `REQ-ARCH-009`'s static check) —
  verified by grep (see below).
- `bsw/mcal/Mcu/{Mcu.h,Mcu.c}` and `bsw/mcal/Port/{Port.h,Port.c}`:
  near-no-op on Ubuntu, validate their config pointer and report to `Det`
  on null, matching a real target's initialization contract shape
  (`REQ-CFG-001`).
- `bsw/services/Det/{Det.h,Det.c,Det_ModuleIds.h}`: bounded, saturating
  per-module error counters plus a 64-record ring buffer, no dynamic
  allocation (verified — see below). `Det_ModuleIds.h` is the shared
  project-local module ID registry other modules extend as they're added.
- `bsw/services/Dem/{Dem.h,Dem.c}`: current status, occurrence counter, and
  last-failed/last-passed monotonic timestamps (from `Gpt`) for the 7
  diagnostic events in spec section 10.2. Documented simplification:
  `occurrenceCount` increments only on a passed→failed transition, not on
  every repeated FAILED report while already failed.
- `bsw/os/Os/{Os.h,Os.c}`: cooperative cyclic-executive scheduler. Absolute
  next-release timestamps (no drift), priority-ordered dispatch when
  multiple tasks are due together (larger numeric priority runs first,
  ties broken by config-array position), missed-period counting with a
  `Det` overrun report, and per-task stats (invocation count, max start
  latency, max exec time, missed count). Deliberately does **not** sleep or
  loop internally — `Os_MainFunction` is one scheduling pass; the real-time
  outer loop is wired into `ecu/*/main.c` in Phase 6. This keeps the
  scheduler pure logic, fully testable with a fake clock.
- `tests/fixtures/Gpt_Fake.{h,c}`: a test-only Gpt substitute (same public
  API, manually-advanced counter instead of the host clock), compiled
  directly into test executables instead of linking `bsw_mcal`, per the
  Phase 1 deliverable "unit tests ... with a fake Gpt" (section 16).
- `test_scheduler` (3 test functions) and `test_det_dem` (4 test
  functions), registered with `ctest`, covering exactly the section 14.1
  bullet points for these two targets.
- Removed the Phase 0 placeholder sources `bsw/mcal/Bsw_Mcal_Placeholder.c`
  and `bsw/services/Bsw_Services_Placeholder.c`, now superseded by real
  content.

### Phase 1 exit criteria (spec section 16) — verification

All verified in the same Ubuntu 20.04 dev sandbox as Phase 0 (see that
phase's deviation note — `cmake_minimum_required` was again temporarily
lowered to 3.16 to run these checks, then restored to `3.20`):

- [x] Build succeeds with **both** GCC 9.4.0 and Clang 10.0.0, zero
      warnings, with the full mandatory warning set.
- [x] `ctest` passes: 3/3 (`test_smoke_phase0`, `test_scheduler`,
      `test_det_dem`), under both compilers.
- [x] `test_scheduler`'s assertions pass using only the fake `Gpt` —
      **deterministic with simulated time**, no wall-clock dependency: due
      ordering (`Task_Io_5ms` before `Task_App_10ms` when both release
      together), missed-period counting (4 missed periods when polled only
      once after 500ms of simulated inactivity), and monotonic deadline
      advancement (next release always exactly one period later, never
      drifting).
- [x] `grep -rl clock_gettime --include=*.c --include=*.h .` returns
      **only** `bsw/mcal/Gpt/Gpt.c` — no other module, including `Os` and
      `Dem` (both need timing), reads the host clock directly.
- [x] `grep -rn 'malloc\|calloc\|realloc\|free(' ...` across
      `bsw/ rte/ asw/ ecu/ platform/ tests/` returns nothing — no dynamic
      allocation anywhere, including the new `Det`/`Dem`/`Os` state (all
      fixed-size static arrays).
- [x] `clang-format --dry-run --Werror` passes on every `.c`/`.h` file
      (found real violations on first run, same brace-style mismatch as
      Phase 0 — fixed with `clang-format -i`, re-verified clean).

## Phase 2 — delivered

- `bsw/mcal/VirtualHw/{VirtualHw.h,VirtualHw.c}`: the shared non-blocking
  `AF_UNIX`/`SOCK_DGRAM` datagram transport. **The second (and last, along
  with the future `Can`) module allowed to touch a host socket
  (`REQ-ARCH-009`)** — verified by grep (see below). Bounded drain
  (`REQ-VHW-002`), malformed/oversized rejection without touching the
  cached payload (`REQ-VHW-003`), SEQ-based duplicate/reorder rejection
  (`REQ-VHW-004`), non-fatal/non-blocking send with no listener
  (`REQ-VHW-005`), and deterministic stale-socket-file cleanup at init
  (`REQ-ECUM-006`). RX bind failures and TX-with-no-listener are both
  reported to `Det` but never fatal (`REQ-ECUM-005`).
- `bsw/mcal/VirtualHw/VirtualHw_Parse.c`: the generic "KEY=value" ASCII
  parser (section 15.5.2's wire format), deliberately split into its own
  pure, stateless, socket-free translation unit so tests can link this real
  implementation alongside a fake `VirtualHw` instead of duplicating it.
- `bsw/mcal/Adc/{Adc.h,Adc.c}` and `bsw/mcal/Dio/{Dio.h,Dio.c}`: raw
  pass-through readers over `VirtualHw` — no scaling, clamping, or
  interpretation (`REQ-MCAL-001`), even for implausible counts (plausibility
  is `IoHwAb`'s job, Phase 5). Both delegate "do we have a value" and "how
  old is it" entirely to `VirtualHw`'s own cache rather than keeping a
  second, independently-timestamped copy — avoiding a class of bug where a
  module's own age tracking could silently reset every time it happens to
  re-poll an unchanged, already-stale value. Extended beyond section 7.4's
  literal minimum API with `Adc_GetGroupAge`/`Dio_GetChannelAge` (documented
  next to each), since `REQ-MCAL-001` requires reporting age but the
  literal signatures given don't carry an age output parameter.
- `bsw/mcal/Display/{Display.h,Display.c}`: transmit-only byte-frame
  transport over `VirtualHw`. Enforces `minWritePeriodMs` (throttles,
  doesn't send, counted separately from failures), counts real send
  failures separately (feeds `DEM_EVENT_DISPLAY_LINK_LOST` in a later
  phase), and is never fatal when nobody is listening (`UC-11`).
- `tests/fixtures/VirtualHw_Fake.{h,c}`: a test-only `VirtualHw` substitute
  (same public API, in-memory state instead of real sockets), used by
  `test_adc_dio`/`test_display` for deterministic control over "the latest
  payload" and send outcomes — the Phase 2 deliverable "unit tests using a
  fake link" (section 16), mirroring Phase 1's `Gpt_Fake` seam.
- `test_virtual_hw` (4 test functions, using the REAL sockets — this is the
  layer that owns them), `test_adc_dio` (4 test functions), and
  `test_display` (4 test functions), registered with `ctest`, covering
  exactly the section 14.1 bullet points for these three targets.
- `tools/hmi/hw_link.py`: standard-library-only manual stimulation tool
  (section 15.6.2) for sending a single or repeating datagram onto a
  virtual hardware link, ahead of the full Tkinter HMI (Phase 7).
- Extended `Det_ModuleIds.h` with `VIRTUALHW`, `ADC`, `DIO`, `DISPLAY`.

### Phase 2 exit criteria (spec section 16) — verification

All verified in the same Ubuntu 20.04 dev sandbox as Phases 0-1 (same
`cmake_minimum_required` temporary-lower/restore pattern):

- [x] Build succeeds with **both** GCC 9.4.0 and Clang 10.0.0, zero
      warnings, with the full mandatory warning set (one real
      `-Wconversion` violation found and fixed on the first attempt — an
      unsigned subtraction narrowed to `uint16` without a cast, in both
      the real and fake `VirtualHw_GetLatestPayload`).
- [x] `ctest` passes: 6/6 (adding `test_virtual_hw`, `test_adc_dio`,
      `test_display`), under both compilers.
- [x] `test_virtual_hw` exercises **real** `AF_UNIX` sockets end to end: a
      raw peer socket created in the test sends datagrams that the real
      driver receives, bounding drain to 8 per activation, keeping the
      latest, rejecting malformed/oversized/duplicate datagrams without
      disturbing the cache, and confirming a send to a path nobody is
      bound to returns promptly with `E_NOT_OK` rather than blocking.
- [x] `test_adc_dio` confirms raw pass-through (including an
      out-of-range count, unclamped), age reporting driven purely by the
      fake `Gpt` (no new datagram needed for age to grow), an unconfigured
      group/channel being rejected with a `Det` report, and the configured
      default `Dio` level before any datagram ever arrives.
- [x] `test_display` confirms a successful write reaches `VirtualHw_Send`
      exactly once, a send failure is counted separately from throttling,
      a write inside `minWritePeriodMs` is throttled and never reaches
      `VirtualHw_Send`, and an oversized frame is rejected before ever
      reaching the transport.
- [x] `grep` for `clock_gettime`/`sys/socket.h`/`sys/un.h`/`AF_UNIX`/
      `linux/can.h` across `asw/ rte/ ecu/ platform/ tools/` (i.e.
      everywhere except `bsw/mcal/` and the explicitly-exempt `tests/`)
      returns nothing (`REQ-ARCH-009`).
- [x] `grep` for a `"/tmp/` path literal in the same scope returns nothing.
- [x] `grep` for `malloc`/`calloc`/`realloc`/`free` across
      `bsw/ rte/ asw/ ecu/ platform/ tests/` still returns nothing.
- [x] `clang-format --dry-run --Werror` passes on every `.c`/`.h` file
      (found the same brace-style violations as Phases 0-1 on new files —
      fixed with `clang-format -i`, re-verified clean).

## Phase 3 — delivered

- `bsw/mcal/Can/{Can.h,Can.c}`: nonblocking `PF_CAN`/`SOCK_RAW`/`CAN_RAW`
  transport, one socket per configured controller (this project uses one
  controller per ECU). **The third module allowed to touch a host socket
  (`REQ-ARCH-009`)**, alongside `Gpt` and `VirtualHw` — verified by grep
  (see below). Bounded drain per `Can_MainFunction_Read` call (mirrors
  `VirtualHw`'s `REQ-VHW-002`), classic-CAN DLC ceiling of 8 enforced on
  both directions, error/RTR frames dropped, and `CAN_CS_STARTED` gating
  on transmit and on forwarding a received frame upward (a stopped
  controller still drains its kernel socket buffer so it cannot silently
  fill up, but never calls `CanIf_RxIndication`). A CAN interface that
  fails to open is exposed via `Can_IsControllerReady` and stays
  permanently `CAN_CS_UNINIT` — unlike a `VirtualHw` link
  (`REQ-ECUM-005`), `REQ-ECUM-001` makes this a startup failure that
  `EcuM` (Phase 6) is expected to check for and abort on. A successful
  `Can_Write` calls `CanIf_TxConfirmation` directly (section 7.1 rule 9);
  `Can_MainFunction_Read` calls `CanIf_RxIndication` directly (section
  7.2's sequence diagram) — both are header-only dependencies from
  `bsw_mcal` into `bsw_com`, resolved at final executable link (same seam
  already used for `Mcu`/`Port`/`Gpt` → `Det`), so `Can.h` itself has no
  `CanIf` dependency and unit tests substitute a fake `CanIf` instead.
- `bsw/communication/CanIf/{CanIf.h,CanIf.c}`: static Tx/Rx PDU routing
  tables (`CanIf_TxPduConfigType`/`CanIf_RxPduConfigType`), validated
  all-or-nothing at `CanIf_Init` — index-consistency check per entry,
  DLC bounds, and rejection of a duplicate physical CAN ID within one Rx
  table (`REQ-CFG-003`). `CanIf_Transmit` rejects an unmapped/out-of-range
  logical Tx PDU id, a length that doesn't match the configured DLC, or a
  controller that isn't `CAN_CS_STARTED`, all before ever calling
  `Can_Write`. `CanIf_RxIndication` rejects an unmapped CAN ID/controller
  pair or a mapped one with the wrong DLC. No socket code anywhere in this
  file (section 3.1: CanIf "may call" only the Can Driver). Phase-3 scope
  note documented in the header: `CanIf_RxIndication`/`CanIf_TxConfirmation`
  only validate, count, and cache the event for test inspection in this
  phase, since `PduR`/`Com` (the next hop per sections 7.1/7.2) are Phase 4
  deliverables that don't exist yet; Phase 4 adds the forwarding call
  without changing this header's signatures.
- Extended `Det_ModuleIds.h` with `CAN`, `CANIF`, and `Det.h` with
  `DET_ERR_HW_INIT_FAILED` (distinct from `DET_ERR_INVALID_CONFIG`: a
  well-formed config table can still fail to open real hardware).
- `tests/fixtures/CanIf_Fake.{h,c}`: a test-only `CanIf` substitute
  implementing only the two notification entry points `Can.c` actually
  calls (`CanIf_RxIndication`, `CanIf_TxConfirmation`), backed by
  last-call recording — used by `test_can` so the Can Driver is tested in
  isolation from CanIf's own routing logic.
- `tests/fixtures/Can_Fake.{h,c}`: a test-only `Can` substitute
  implementing the full `Can.h` public API over in-memory per-controller
  state, plus a `CanFake_*` control surface (force a controller mode or a
  `Can_Write` result without going through real validation) — used by
  `test_canif` so CanIf's routing/validation logic is tested
  deterministically without a real SocketCAN interface.
- `test_can` (6 test functions) — uses the REAL `Can.c` against **real**
  `vcan0` (this is the layer that owns the socket, same rationale as
  `test_virtual_hw`) with a FAKE `CanIf`. `test_canif` (9 test functions)
  — uses the REAL `CanIf.c` against a FAKE `Can`. Both registered with
  `ctest`, covering exactly the section 14.1 bullet points for these two
  targets and the Phase 3 exit criteria's "wrong ID/DLC/controller-state"
  cases.

### Phase 3 exit criteria (spec section 16) — verification

All verified in the same Ubuntu 20.04 dev sandbox as Phases 0-2 (same
`cmake_minimum_required` temporary-lower/restore pattern):

- [x] Build succeeds with **both** GCC 9.4.0 and Clang 10.0.0, zero
      warnings, with the full mandatory warning set. Two real build errors
      found and fixed on the first attempt: (1) `struct ifreq` was
      undeclared (`_POSIX_C_SOURCE` alone disables glibc's BSD-ism gate;
      fixed by also defining `_DEFAULT_SOURCE` in `Can.c` and
      `test_can.c`), and (2) a `-Wconversion` narrowing of `PduLengthType`
      (`uint16`) to `Can_PduType.length` (`uint8`) in `CanIf_Transmit`
      (fixed with an explicit cast, safe because it is only reached after
      confirming the length already equals the configured `uint8` DLC).
- [x] `ctest` passes: 8/8 (adding `test_can`, `test_canif`), under both
      compilers.
- [x] **Loopback test on `vcan0` passes**: `test_can` writes a frame and an
      independent peer raw socket (playing "the other ECU") observes it
      on the bus with the correct CAN ID/DLC/payload — deliberately
      structured as two parties rather than self-reception, since
      `vcan0`'s default `CAN_RAW_RECV_OWN_MSGS=0` means a socket does not
      see its own transmitted frames (exactly what lets the real
      `SensorEcu`/`ClusterEcu` processes talk over `vcan0` with no real
      bus hardware); a separate case has the peer inject a frame that
      `Can_MainFunction_Read` drains and forwards.
- [x] Wrong-DLC case tested at both layers: `Can_Write` rejects a length
      greater than 8 before touching the socket; `CanIf_Transmit` and
      `CanIf_RxIndication` each reject a length that doesn't match the
      configured DLC.
- [x] Wrong-ID case tested at both layers: `CanIf_Transmit` rejects an
      out-of-range/unmapped logical Tx PDU id; `CanIf_RxIndication`
      rejects a CAN ID with no matching configured Rx PDU.
- [x] Wrong-controller-state case tested at both layers: `Can_Write` and
      `CanIf_Transmit` both reject a transmit while the controller isn't
      `CAN_CS_STARTED`; a real frame injected onto `vcan0` while stopped
      is drained (counted) but never reaches `CanIf_RxIndication`, then
      the very next frame after starting the controller does.
- [x] `Can_IsControllerReady`/`Can_SetControllerMode` confirm
      `REQ-ECUM-001`: a controller configured against a nonexistent
      interface name never becomes ready and can never be started later.
- [x] `grep` for `clock_gettime`/`sys/socket.h`/`sys/un.h`/`linux/can.h`/
      `linux/can/raw.h`/`AF_UNIX`/`AF_CAN`/`PF_CAN` across `asw/
      bsw/communication/ bsw/ecu_abstraction/ bsw/os/ bsw/services/ rte/
      ecu/ platform/ tools/` (i.e. everywhere except `bsw/mcal/` and the
      explicitly-exempt `tests/`) returns nothing (`REQ-ARCH-009`).
- [x] `grep` for a `"/tmp/` path literal in the same scope returns nothing.
- [x] `grep` for `malloc`/`calloc`/`realloc`/`free` across
      `bsw/ rte/ asw/ ecu/ platform/ tests/` still returns nothing.
- [x] `clang-format --dry-run --Werror` passes on every `.c`/`.h` file
      (found the same brace/line-wrap style violations as Phases 0-2 on
      new files — fixed with `clang-format -i`, re-verified clean).

#### Deviation note: `test_can` requires `vcan0`

Unlike every prior test target, `test_can` opens a real socket on
`vcan0` and fails to build meaningfully as a test if that interface is
absent — documented in `README.md`'s "Build and test" section as a
prerequisite carried over from "Virtual CAN interface" (`tools/setup_vcan.sh`).
This is intentional per section 16's own exit criterion ("loopback test on
vcan passes"): unlike `VirtualHw` links, there is no fake substitute for
the layer that owns the socket itself, matching `test_virtual_hw`'s same
real-socket rationale in Phase 2.

## Phase 4 — delivered

- `bsw/communication/Com/ComProtection.{h,c}`: pure, stateless CRC8
  (CRC-8/AUTOSAR: polynomial `0x1D`, init `0xFF`, no reflection, final
  XOR `0xFF`) and alive-counter acceptance rule, split out of `Com.c`'s
  stateful I-PDU logic the same way `VirtualHw_ParseUint` was split out
  in Phase 2 — so it can be tested directly against section 6.5's three
  golden vectors with no Det/Gpt/Com dependency at all.
- `bsw/communication/Com/{Com.h,Com.c}`: signal shadow buffers, bit-level
  pack/unpack using shifts and masks only (`REQ-COM-006`, no struct cast
  onto the wire), cyclic Tx with absolute next-release timestamps (same
  drift-free idiom as `Os_MainFunction`), and Rx validation in the exact
  order section 6.5/7.2 require: DLC (`REQ-COM-002`) before CRC
  (`REQ-COM-003`) before the alive counter before every signal's range
  (`REQ-COM-004`) — all decoded into a local buffer and validated before
  any state is committed, so a single invalid frame can never partially
  update signals (`REQ-ASW-005`). Rx deadline supervision
  (`REQ-COM-005`) is a separate `Com_MainFunctionRx` pass over a
  monotonic last-valid-reception timestamp, queryable via
  `Com_GetIpduRxTimeoutStatus`. A zero-argument `Com_SignalNotificationType`
  callback (matching real AUTOSAR's `<Signal>_Rx_Notification` shape)
  fires per signal on a fully valid receive, standing in for the RTE
  that doesn't exist until Phase 5. Config validation is all-or-nothing
  at `Com_Init` (index consistency, bit-overlap detection between
  signals and against the counter/CRC bytes, DLC/period/timeout
  sanity) — `REQ-CFG-001/003`. Two `_Static_assert`s pin down the
  fixed-width type widths and the 8-byte wire length (`REQ-CFG-002`;
  see the file's comment on why there's no `sizeof(struct) == 8` to
  assert against directly, given `REQ-COM-006`).
- `bsw/communication/PduR/{PduR.h,PduR.c}`: id-only static routing
  between `Com` and `CanIf` (`REQ-ARCH-003` — never reads past a
  `PduInfoType`'s length/pointer). `PduR_CanIfTxConfirmation` resolves
  the COM-side id via a *reverse* lookup on the original Tx route (by
  its CanIf-side destination), since a confirmation flows back along the
  path the transmit request took.
- `bsw/communication/CanIf/CanIf.c` now actually forwards: an accepted
  `CanIf_RxIndication` calls `PduR_CanIfRxIndication`, and
  `CanIf_TxConfirmation` calls `PduR_CanIfTxConfirmation` — completing
  the section 7.1/7.2 chains that Phase 3 had stubbed with a comment.
- **Real bug found and fixed**: `CanIf_Init`, `Com_Init`, and `PduR_Init`
  all validate their config and return early on the first problem found,
  but none of them reset their own "initialized" flag *before*
  validating. A module re-initialized with a bad config after a prior
  *successful* init would silently keep operating on the old config
  instead of becoming uninitialized. Fixed by setting `initialized =
  FALSE` unconditionally as the first statement in all three `_Init`
  functions, before any validation runs. Found by `test_pdur`'s
  duplicate-route test failing when run after another test's successful
  `PduR_Init` left stale state behind — a genuine cross-test-order bug,
  not a test artifact, since the same failure mode is reachable in
  production by any two-call re-init sequence.
- Extended `Det_ModuleIds.h` with `PDUR`, `COM`.
- `tests/fixtures/Com_Fake.{h,c}` (new), `tests/fixtures/PduR_Fake.{h,c}`
  (new), and `tests/fixtures/CanIf_Fake.{h,c}` extended with a
  `CanIf_Transmit` fake role — `CanIf_Fake` now serves two different
  test roles for the same real module depending on which direction is
  under test (Phase 3's `CanIf_RxIndication`/`CanIf_TxConfirmation` role
  for `test_can`, and a new `CanIf_Transmit` role for `test_pdur`).
- `test_com_protection` (3 test functions, all three section 6.5 golden
  vectors plus counter-validity edge cases), `test_com_pack` (11 test
  functions, real `Com.c`/`ComProtection.c` against a fake `PduR` and
  fake `Gpt` — directly exercises the Phase 4 exit criteria), `test_pdur`
  (9 test functions, real `PduR.c` against a fake `CanIf`/`Com`) —
  registered with `ctest`. `test_canif` extended with a `CanIf_TxConfirmation`
  forwarding test and forwarding assertions added to its existing Rx
  tests, now linking a fake `PduR` too.

### Phase 4 exit criteria (spec section 16) — verification

All verified in the same Ubuntu 20.04 dev sandbox as Phases 0-3 (same
`cmake_minimum_required` temporary-lower/restore pattern):

- [x] Build succeeds with **both** GCC 9.4.0 and Clang 10.0.0, zero
      warnings, with the full mandatory warning set. One real
      `-Wsign-conversion` (part of `-Wconversion` for C) violation found
      and fixed on the first attempt: `(value >> i) & 0x1U` implicitly
      converts the shift's promoted `int` result to `unsigned int`;
      fixed with an explicit `(uint16)`/`(uint8)` cast on the shift
      result before the mask in both `PackBits` and `UnpackBits`.
- [x] `ctest` passes: 11/11 (adding `test_com_protection`, `test_com_pack`,
      `test_pdur`), under both compilers -- but only after fixing the
      real cross-test-order `_Init` staleness bug described above, first
      caught by `test_pdur`.
- [x] **"COM Tx produces exact expected bytes"**: `test_com_pack`'s
      `TestTxProducesExactGoldenBytes` sends `VehicleSpeedRaw=1234,
      IgnitionOn=1` and asserts the exact 8 bytes handed to
      `PduR_ComTransmit` equal section 6.5's golden vector
      `D2 04 01 00 19 00 00 00` byte-for-byte.
- [x] **"raw CAN injection reaches a mock RTE callback"**:
      `TestRxRawInjectionReachesMockRteCallback` hands section 6.5's
      golden-vector bytes for `ClusterStatus` directly to
      `Com_RxIndication` (playing the role of "CanIf/PduR already
      delivered this raw frame", which Phase 3 tested separately) and
      confirms a mock notification callback fires once per signal owned
      by that PDU, with each signal then reading back its correctly
      decoded value via `Com_ReceiveSignal`.
- [x] `test_com_pack` additionally confirms: the alive counter advances
      only after a successful `PduR_ComTransmit` call, not after a
      failed one; cyclic Tx respects its configured period (no early
      resend, no drift); nothing transmits before
      `Com_IpduGroupStart` is called; `Com_SendSignal` rejects an
      out-of-range value; a wrong DLC/CRC/skipped-counter/out-of-range
      signal each invalidate the whole Rx PDU without touching previous
      signal values or firing a notification (REQ-ASW-005), and the
      correctly-sequenced frame right after a rejected one is still
      accepted; Rx timeout supervision reports not-timed-out immediately
      after a valid receive and timed-out once the configured deadline
      elapses with no new valid frame.
- [x] `test_pdur` confirms: `PduR_ComTransmit` forwards to
      `CanIf_Transmit` with the resolved id and unchanged payload and
      forwards a downstream failure rather than swallowing it;
      `PduR_CanIfRxIndication` forwards to `Com_RxIndication`;
      `PduR_CanIfTxConfirmation` forwards to `Com_TxConfirmation` via the
      reverse route lookup; an unmapped id on any of the three is
      rejected without forwarding; a duplicate route source is rejected
      at `PduR_Init`; all three entry points reject calls before Init.
- [x] `grep` for `clock_gettime`/`sys/socket.h`/`sys/un.h`/`linux/can.h`/
      `linux/can/raw.h`/`AF_UNIX`/`AF_CAN`/`PF_CAN`/a `"/tmp/` literal
      across `asw/ bsw/communication/ bsw/ecu_abstraction/ bsw/os/
      bsw/services/ rte/ ecu/ platform/ tools/` (i.e. everywhere except
      `bsw/mcal/` and the explicitly-exempt `tests/`) returns nothing
      (`REQ-ARCH-009`) — unchanged from Phase 3 since Com/PduR touch no
      socket or CAN-specific code at all.
- [x] `grep` for `malloc`/`calloc`/`realloc`/`free` across
      `bsw/ rte/ asw/ ecu/ platform/ tests/` still returns nothing.
- [x] `clang-format --dry-run --Werror` passes on every `.c`/`.h` file
      (found the same brace/line-wrap style violations as every prior
      phase on new and modified files — fixed with `clang-format -i`,
      re-verified clean).

## Phase 4.5 — delivered (SysTimer / Os_Counter split, not a spec phase)

Requested between Phase 4 and Phase 5: separate the OS's time source from
`Gpt`, matching real AUTOSAR (a dedicated hardware timer for the OS,
`Gpt` for everything else). Spec updated to v1.2 (section 0.3) with the
new module, new `REQ-ARCH-014`, and the section 9.2/9.3 init/shutdown
order revised accordingly.

- Created `bsw/mcal/SysTimer/` (`SysTimer.c/.h`, `SysTimer_Cfg.c/.h`):
  reads `CLOCK_MONOTONIC` independently of `Gpt.c`, its own start
  timestamp, never calls into `Gpt`.
- Created `bsw/os/Os/Os_Counter.c/.h`: the OSEK-style counter layer that
  reads `SysTimer` and converts ticks to milliseconds. Never calls
  `Gpt_*` — enforced by a new static check (grep for `Gpt_` under
  `bsw/os/`).
- Rewired `Os.c`/`Os.h` to derive all timing from `Os_Counter`
  (`Os_TaskStatsType.nextReleaseMs` changed type from `Gpt_ValueType` to
  `Os_MilliType`, since `Os.h` no longer includes `Gpt.h`).
- `Gpt` keeps its exact API and continues to serve Com, IoHwAb, Dem,
  Display, and Logger — unchanged.
- New fixture `tests/fixtures/SysTimer_Fake.c/.h`; `test_scheduler`
  switched from the fake `Gpt` to the fake `SysTimer`.
- New tests `test_systimer` (real driver against the real host clock,
  like `test_virtual_hw`/`test_can`) and `test_os_counter` (real counter
  against the fake `SysTimer`).
- **Real bug found**: `Os_Counter` originally had no `_DeInit`, so
  `test_os_counter`'s "read before init" case failed once a prior test in
  the same binary had already called `Os_Counter_Init` — added
  `Os_Counter_DeInit`, which Phase 6's `EcuM` shutdown also needed for
  the reverse-order teardown (section 9.3).
- Exit check: 13/13 tests, `clang-format` clean, `bsw/os/` grep for
  `Gpt_` returns nothing, `clock_gettime` still only in `bsw/mcal/`.

## Phase 5 — delivered (IoHwAb, RTE, SW-Cs)

- `bsw/ecu_abstraction/IoHwAb/` (`IoHwAb.c/.h`, `IoHwAb_Cfg.c/.h`):
  linear ADC-counts-to-km/h scaling with 32-bit intermediates, a
  plausibility window, `IOHWAB_INPUT_TIMEOUT_MS` staleness, and
  `IoHwAb_SetClusterDisplay` which formats the section 15.5.2 display
  line and calls `Display_Write`. Two static configs,
  `IoHwAb_ConfigSensor`/`IoHwAb_ConfigCluster` (`REQ-ARCH-005`).
- `rte/common/Rte_Type.h`: the section 4.3/5.2 application types
  (`VehicleSpeedRawType`, `ClusterStateType`, `Rte_ReturnType`, …) shared
  by both SW-Cs and both RTEs — the only header, besides its own
  generated one, a SW-C may include (`REQ-ARCH-001`).
- `rte/generated/SensorEcu/Rte_SensorEcu.c` and
  `rte/generated/ClusterEcu/Rte_ClusterEcu.c`: full generated-style RTE
  implementations — signal mapping to `Com_SendSignal`/`Com_ReceiveSignal`
  (`REQ-RTE-004`), reception health derived from COM's counters
  (never/timeout/invalid/OK), the client-server adapter onto `IoHwAb`.
- `asw/VehicleStateSwc/VehicleStateSwc.c` and
  `asw/ClusterControlSwc/ClusterControlSwc.c`: the two SW-Cs, each
  including only its own header and its generated `Rte_*.h`
  (`REQ-ARCH-002`). `ClusterControlSwc` implements the full section 4.4
  state machine with hysteresis and the three-frame recovery rule.
- New fixtures: `Adc_Fake`, `Dio_Fake`, `Display_Fake`, `IoHwAb_Fake`,
  `Rte_ClusterFake`; `Com_Fake` extended with a second role (the
  signal-API surface the RTE calls, alongside its original PduR-facing
  role).
- New tests: `test_iohwab`, `test_rte_sensor`, `test_rte_cluster`,
  `test_cluster_swc` — 40 cases total.
- **Real bugs found and fixed**:
  1. `Com_GetIpduRxTimeoutStatus` returns `boolean` (`TRUE` = success), not
     `Std_ReturnType`; both RTEs originally compared it against `E_OK`
     (which is `0`, i.e. `FALSE`), silently disabling timeout detection
     entirely. Fixed to compare against the boolean contract directly.
  2. `IoHwAb_MainFunction` reported `DEM_EVENT_SENSOR_LINK_LOST` once per
     signal instead of once per sensor line; since one virtual hardware
     datagram carries both the ADC and DIO values, the DIO sample's
     verdict silently overwrote the ADC sample's. Fixed to aggregate one
     verdict per activation across every signal on the ECU.
- Exit check: 17/17 tests; REQ-ARCH-002/008 verified by grep (no SW-C
  file includes a BSW/MCAL header or calls `IoHwAb_`/`Adc_`/`Dio_`/`Display_`).

## Phase 6 — delivered (EcuM, static ECU config, executables, signals)

- `bsw/services/Logger/` (new, named by sections 3.1/9.2/13.4): the
  section 13.4 log line format, gated so it never reads `Gpt` before
  `Gpt_Init` has run (the logger starts at init step 1, `Gpt` at step 6).
- `bsw/services/EcuM/EcuM.c/.h`: one generic implementation for both
  ECUs (`REQ-ARCH-005`) — a config struct of function-pointer-free,
  mostly-`const`-pointer fields where a `NULL` module pointer means "this
  ECU doesn't have that module". Runs the full section 9.2 order
  (19 steps once `SysTimer`/`Os_Counter` are counted separately) and the
  section 9.3 reverse order, including the scheduler/DET summary print.
- `ecu/SensorEcu/config/SensorEcu_Cfg.c` and
  `ecu/ClusterEcu/config/ClusterEcu_Cfg.c`: the link-time configuration
  tables (CanIf Tx/Rx PDUs, PduR routes, COM signals/I-PDUs mirroring the
  golden-vector layout from `test_com_pack`, the five section 8.2 tasks)
  and the task bodies themselves. The only files on each ECU that know
  both a CAN ID and a socket path.
- `ecu/SensorEcu/main.c` / `ecu/ClusterEcu/main.c`: argument parsing
  (`--can-if`, `--sensor-link`/`--display-link`, `--duration`,
  `--log-level`, `--help`), `SIGINT`/`SIGTERM` handlers that only set an
  atomic flag (`REQ-ECUM-003`), and the outer real-time loop.
- **Real bugs found by actually running both ECUs together**:
  1. The section 9.3 step-8 scheduler summary read `Os_GetTaskStats`
     *after* `Os_DeInit`, so it printed all-zero stats and produced 5 Det
     errors instead of a report. Fixed by deferring `Os_DeInit` to step 9
     (leaving `ECUM_STATE_RUN` already stops new activations).
  2. The outer loop slept a flat 1 ms between `EcuM_MainFunction` calls;
     `nanosleep` always overshoots its request, so the 1 ms bus task
     missed its own release on most iterations — 59 scheduler-overrun Det
     errors per second of runtime. Fixed by polling at a quarter of the
     shortest task period (250 µs) instead of the period itself; verified
     0 missed activations over a 2-second run.
  3. `DEM_EVENT_*` transition logging (added for section 15.8, see below)
     skipped the very first transition of any event that was already
     FAILED the first time it was observed (the normal case for a sensor
     line with nothing plugged in yet) — a "seeded" flag gated the whole
     first pass instead of just priming the previous-value table. Fixed
     by seeding the table to PASSED instead.
- Section 15.8 compliance added: `ClusterControlSwc` state transitions
  and both ECUs' DEM status transitions are logged with a timestamp, but
  only on change — verified this does not violate the section 13.4
  bounded-log-volume rule.
- `tests/unit/test_config.c` (spec section 14.1 "test_config"): validates
  the actual shipping SensorEcu tables (accepted, zero Det errors, wired
  end to end) plus duplicate-Rx-CAN-ID, PDU-index-mismatch,
  duplicate-route-source, missing-route-at-runtime,
  overlapping-signal-bits, signal-on-protection-field, wrong-DLC, and
  Os-task-table rejection cases.
- `tests/integration/` (new): the spec's own Phase 6 exit criterion is
  "IT-01, IT-02, IT-05, IT-06, and IT-07 pass" — not merely "observed to
  work" — so these are automated, not manual. `it_common.py` is the
  shared harness (starts the real `sensor_ecu`/`cluster_ecu` binaries as
  subprocesses, drives them through real `AF_UNIX` virtual hardware
  links and the real `vcan0`, tears down with `SIGTERM`). Wired into
  CTest under the `integration` label (`ctest -L integration`; excluded
  from a bare `ctest` run since each test takes several real seconds,
  unlike the millisecond unit tests):
  - `test_it01_normal_e2e`: both ECUs plus `scenario_player.py` for 4s;
    asserts `0x180`/`0x181` appear at their configured rates and the
    cluster visits OFF, NORMAL, and OVERSPEED per the scenario.
  - `test_it02_timeout_recovery`: stops `sensor_ecu`, asserts
    `COMM_FAULT` within a documented margin around the spec's 120 ms
    claim; restarts it, asserts recovery.
  - `test_it05_virtual_hw_input`: a scripted client sends 120.00 km/h
    onto the sensor line; asserts `12000 ± 4` raw units on `0x180`, a
    steady-state Tx gap no larger than 3× the 20 ms period, `OVERSPEED`
    on the bus and the display line together, and the 0/2048/4095-count
    conversion sweep from section 4.1.2.
  - `test_it06_sensor_disconnect`: stops sending on the sensor line
    (CAN keeps running); asserts `DEM_EVENT_SENSOR_LINK_LOST` fails,
    `SensorEcu` publishes speed 0/ignition off, and — the actual point of
    the test — `ClusterEcu` does **not** enter `COMM_FAULT`, since a dead
    sensor is not a dead bus; then asserts recovery.
  - `test_it07_display_absent`: starts `ClusterEcu` with no display
    listener for 3s (shortened from the spec's 10s to keep the suite
    fast; the assertion — no crash, no stall, state machine unaffected —
    is checked continuously throughout, not only at the end), asserts
    `DEM_EVENT_DISPLAY_LINK_LOST` fails and the state machine is
    unaffected, then attaches a listener and asserts recovery.
- **Real bug found by IT-06** (only visible once a test actually read a
  *running* ECU's log rather than just its final state): `Logger_Log`
  never flushed `stdout`, and glibc fully buffers `stdout` whenever it is
  not a TTY — the normal case for every demo script, every integration
  test, and any real deployment under a supervisor. Every log line sat
  in a libc buffer until the process exited, silently breaking section
  15.8's "ECU stdout logs" observation surface for as long as the ECU
  kept running. Fixed with an `fflush(stdout)` per line in `Logger_Log`
  (section 13.4 already bounds log volume, so the cost is negligible).
- Exit check: 18 unit + 5 integration tests, 23/23; REQ-ECUM-001 verified
  by construction (CanIf `CAN_CS_STARTED` failure is the only path that
  returns E_NOT_OK from `EcuM_Init` after `Can_Init`); REQ-ECUM-004
  verified by calling `EcuM_Shutdown` twice in a row without a crash.
- **Documented spec deviation**: `ecu/<Ecu>/main.c` contains the literal
  default socket paths (`/tmp/vhw_sensor.sock`, etc.) required by section
  15.4's `--sensor-link`/`--display-link` defaults. REQ-ARCH-009's
  static check (section 14.3) is amended with an explicit exemption for
  exactly this one file per ECU: the literal is an argument default
  handed down into MCAL configuration, not a module above MCAL opening a
  link itself.

## Phase 7 — delivered (tools/hmi/, scenario player, demo scripts)

- `tools/hmi/scenario_player.py`: parses and fully validates a CSV
  against section 15.7 *before* sending a single datagram (malformed
  input exits nonzero with a line number and sends nothing), then plays
  step changes at their scheduled times with the `REQ-VHW-007` 100 ms
  keepalive; `--loop` and `--speed-factor` implemented.
- `tools/hmi/can_monitor.py`: read-only `AF_CAN`/`CAN_RAW` trace of
  `vcan0`, decoding `0x180`/`0x181` per sections 6.3/6.4 independently of
  any ECU log — the third of the section 15.8 observation surfaces.
- `tools/hmi/vehicle_hmi.py`: the full Tkinter rig — sensor slider
  (0.5 km/h step) plus numeric entry, ignition toggle, two fault-injection
  checkboxes (disconnect the line; send out-of-range counts, per
  `REQ-HMI-007`'s hardware-only fault-injection limit), the instrument
  panel (big speed readout, state, three lamps, link-age), and a 200-row
  CAN trace. Three daemon worker threads (`SensorRigSender`,
  `DisplayLineReceiver`, `CanTraceReader`) push events through a
  `queue.Queue`; only `Tk.after()` at 100 ms touches widgets
  (`REQ-HMI-002`).
- `tools/run_demo.sh`: starts `cluster_ecu`, `sensor_ecu`, and either the
  HMI or `scenario_player.py --headless` with matching link paths;
  `tools/inspect_can.sh`: prefers `candump` if installed, else falls back
  to `can_monitor.py`.
- `scenarios/default.csv` (the spec's own example) and
  `scenarios/timeout_recovery.csv` (new, exercises a longer hold at
  speed and a higher-speed excursion for manual timeout/recovery
  observation).
- **Real bugs found**:
  1. Every Phase 7 script (and the pre-existing `hw_link.py` from an
     earlier phase, which had never actually been run on this machine)
     used PEP 585 generics (`list[str]`) evaluated eagerly; the sandbox's
     Python is 3.8, so every script crashed on import with `TypeError:
     'type' object is not subscriptable` before printing so much as
     `--help`. Fixed with `from __future__ import annotations` in all
     four files — this is exactly the failure class continuous testing
     against the real interpreter is supposed to catch, and it had been
     latent since whichever earlier phase wrote `hw_link.py`.
  2. `vehicle_hmi.py`'s three worker-thread classes stored their stop
     flag as `self._stop = threading.Event()`, which silently shadows
     `threading.Thread`'s own private `_stop()` method — `Thread.join()`
     then raises `TypeError: 'Event' object is not callable`. Caught by
     a headless smoke test that starts a worker thread and joins it (no
     display available in this environment to run the GUI itself).
     Renamed to `self._stop_event`.
  3. `speed_kph_to_counts` needed a documented negative-input clamp
     (`hw_link.py`/`scenario_player.py`/`vehicle_hmi.py` all share the
     same formula); verified by the headless check rather than assumed.
- Verified live: `scenario_player.py --scenario scenarios/default.csv`
  driving a real `sensor_ecu` end to end reproduces the exact state
  sequence above; `can_monitor.py`/`inspect_can.sh` independently show
  the `0x180`/`0x181` frames with correct alive-counter/CRC progression.
  `vehicle_hmi.py` itself could not be visually verified (no display/Xvfb
  in this environment) — its socket/threading logic was verified with a
  headless script that exercises `SensorRigSender`, `DisplayLineReceiver`,
  `parse_display_frame`, and `decode_can` directly; `pyflakes` and
  `ast.parse` are clean on all four files.
- Exit check: `pyflakes tools/hmi/*.py` clean; `bash -n` clean on both
  new shell scripts; REQ-ARCH-011 (no ECU source import) and REQ-HMI-001
  (stdlib only) verified by grep.

## Phase 8 — delivered (robustness and handoff)

- **IT-03 Integrity rejection** (`tests/integration/test_it03_integrity_rejection.py`):
  hand-builds a `0x180` frame with a deliberately wrong CRC using an
  independent Python re-implementation of CRC-8/AUTOSAR
  (`it_common.compute_crc8`, verified against all three of section 6.5's
  golden vectors before being trusted), injects it directly onto `vcan0`
  with a raw `PF_CAN`/`SOCK_RAW` socket (`RawCanSender`) while `sensor_ecu`
  is not even running, and asserts the corrupt value never reaches
  `ClusterStatus` and `DEM_EVENT_VEHICLE_STATUS_INTEGRITY` fails; then
  sends 3 valid frames and asserts recovery.
  - **Real bug found**: none of the four COM-layer DEM events
    (`DEM_EVENT_VEHICLE_STATUS_TIMEOUT`, `DEM_EVENT_VEHICLE_STATUS_INTEGRITY`,
    `DEM_EVENT_CLUSTER_STATUS_TIMEOUT`, `DEM_EVENT_CLUSTER_STATUS_INTEGRITY`)
    were ever actually set anywhere in the codebase — they were defined in
    Phase 1 (`Dem.h`) and *read* since Phase 6 (the status log), but
    nothing ever called `Dem_SetEventStatus` for them, a gap dating back
    at least to Phase 6 that no earlier unit test could catch (COM's own
    tests use a fake `Dem`-free config; the RTE tests use a fake `Com`
    that never triggers real integrity/timeout detection). Fixed by
    adding the calls to both RTEs' `UpdateReceptionHealth` (the one place
    that already computes both conditions every cycle), which is also
    the correct, spec-literal home per section 10.2's table ("Cluster
    COM protection" / "Sensor COM/Rx supervision").
  - **Test-design note**: IT-03's literal text ("application value is
    unchanged") is ambiguous against section 4.4's own state diagram
    ("`* --> COMM_FAULT: timeout or invalid data`") and the already-tested
    `REQ-ASW-001`. Resolved by checking "unchanged" at the layer
    `REQ-COM-003`/`REQ-ASW-005` actually govern — the corrupt frame's own
    garbage value must never leak into `ClusterStatus` — while accepting
    that the cluster SW-C correctly transitions to `COMM_FAULT` (display
    0) on any single invalid reception, exactly as `test_cluster_swc`'s
    `TestCommunicationInvalidityOutranksEverything` already established
    in Phase 5.
- **IT-04 Layered Tx/Rx evidence** (`tests/integration/test_it04_layered_evidence.py`):
  added the two counters the spec explicitly asks for that didn't exist
  yet (`PduR_GetComTransmitForwardCount`, `PduR_GetCanIfRxIndicationForwardCount`,
  `Adc_GetGroupReadCount`, `IoHwAb_GetValidSampleCount`); wired every
  layer's counter (RTE, COM, PduR, CanIf, Can, Adc, IoHwAb) into a new
  bounded, once-per-second `[Layers]` log line on both ECUs (section 13.4
  compliant); the test reads that same line back — "test instrumentation,
  not an alternate data path", exactly as the requirement specifies — and
  additionally cross-checks that `ClusterEcu`'s received-frame count never
  exceeds `SensorEcu`'s confirmed-sent count, proving the counters are
  evidence of the *same* exchange, not two coincidentally-incrementing
  numbers.
- **Sanitizer run**: a separate build tree
  (`cmake -S . -B build-asan -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"`)
  builds cleanly and all 19 unit tests plus all 7 integration tests
  (`IT_BUILD_DIR=build-asan` env var, added to `tests/integration/it_common.py`
  for exactly this purpose) pass with zero AddressSanitizer or
  UndefinedBehaviorSanitizer diagnostics (checked explicitly by grepping
  every ECU log for sanitizer output, not just trusting exit codes).
  - **Real, reproducible finding**: `bsw/communication/Com/Com.c`'s
    `PackBits`/`UnpackBits` — verified `-Wsign-conversion`-clean back in
    Phase 4 by an explicit standalone experiment — fail that exact same
    check under `-fsanitize=undefined` with the identical GCC 9.4.0 and
    identical warning flags otherwise, confirmed by isolating the
    difference to the sanitizer flag alone (`/tmp/sign_test3/4.c`, since
    deleted). Fixed by assigning the shifted value to a named same-width
    variable before the mask, instead of an inline cast immediately
    before `&` — empirically verified clean under both flag sets this
    time, not just the one that happened to be tested before.
- **`REQ-ARCH-012` stub-MCAL build**: added a CMake cache variable
  `AUTOSAR_MCAL_VARIANT` (`real`, the default, or `stub`) that swaps only
  `bsw/mcal/Can/Can.c` and `bsw/mcal/VirtualHw/VirtualHw.c` for
  `Can_Stub.c`/`VirtualHw_Stub.c` — new files implementing the identical
  headers with an in-memory loopback queue and a single-slot latest-value
  cell respectively, no `socket`/`clock_gettime` call anywhere in either.
  Every other file, including `Adc.c`/`Dio.c`/`Display.c` (which already
  only ever called `VirtualHw_*`, never a socket directly) and everything
  in `rte/`/`asw/`/`ecu/`, is untouched.
  - `tests/unit/test_stub_mcal.c` (new, always built, independent of the
    CMake variant switch): links the REAL `Adc.c`/`Dio.c`/`CanIf.c`
    completely unmodified against the STUB `Can_Stub.c`/`VirtualHw_Stub.c`,
    proving the substitution seam itself — 12 checks, all passing.
  - Full verification: configured and built `build-stubmcal/` with
    `-DAUTOSAR_MCAL_VARIANT=stub` — clean build, zero errors/warnings, both
    `sensor_ecu` and `cluster_ecu` produced. Ran each for 2 real seconds:
    full section 9.2 init order completes, the section 8.2 task schedule
    runs with 0-1 missed activations (same as the real-MCAL build), COM Tx
    counters advance normally against `Can_Stub`, and `SIGTERM` shutdown
    completes cleanly (exit 0) with the full section 9.3 reverse-order
    teardown and scheduler summary printed. `test_can`/`test_virtual_hw`
    are unaffected by the switch (their own `add_executable` always lists
    the real `.c` files directly, since they exist specifically to test
    the real socket-based drivers).
  - **Documented, expected side effect, not a bug**: each stub-built ECU
    logs ~50 `CanIf` `DET_ERR_INVALID_PARAM` records per second of
    runtime. Cause: `Can_Stub`'s loopback queue is per-process, so each
    ECU's own transmitted frame loops back to its own
    `Can_MainFunction_Read` and reaches `CanIf_RxIndication` with a CAN ID
    that ECU never configured itself to receive (`SensorEcu` only expects
    `0x181` on Rx, `ClusterEcu` only `0x180`) — a condition that can never
    happen on a real two-bus topology (`vcan0`, or two wires), where an
    ECU only ever hears the *other* ECU's frames. Documented here rather
    than silently filtered, since a stub-only Det pattern is exactly the
    kind of detail a real microcontroller port needs to know does NOT
    carry over.
- Full traceability: every one of the spec's 69 distinct `REQ-*`
  identifiers now has at least one row in
  `docs/REQUIREMENTS_TRACEABILITY.md` (verified by diffing the set of IDs
  grepped from the spec against the set grepped from the traceability
  table — empty diff both directions). Five requirements
  (`REQ-ARCH-004/006/007/010`, plus the newly-implemented `REQ-ARCH-012`)
  were already true of the code but had never been given a row in any
  earlier phase's pass; added under a new "Requirements not tied to one
  phase's own section" heading rather than retroactively editing every
  earlier phase section.
- Architecture and test documents updated to match implementation:
  this file, `docs/TEST_PLAN.md`, and `docs/REQUIREMENTS_TRACEABILITY.md`
  all updated for Phase 8; `docs/ARCHITECTURE.md` was already current
  (updated per-phase throughout, per the standing convention).
- Exit check: 26/26 tests (19 unit + 7 integration) on the normal build;
  19/19 unit + 7/7 integration under ASan+UBSan with zero diagnostics;
  `test_stub_mcal` passes and both real ECU executables run their full
  lifecycle correctly against the stub MCAL variant; `clang-format` clean
  across every `.c`/`.h` touched this phase; every spec `REQ-*` id has a
  traceability row.

## Phase 9 — delivered, with known open issues (v2.0)

- `bsw/services/BswM/{BswM.h,BswM.c}`: startup/shutdown action lists and
  mode arbitration per spec section 9.4, triggered from `EcuM`.
- `bsw/services/ComM/{ComM.h,ComM.c}`: communication-mode users and
  arbitrated channel mode.
- `bsw/communication/CanSM/{CanSM.h,CanSM.c}`: per-channel CAN state
  machine translating a `ComM` mode into `CanIf` controller-mode calls.
- `bsw/mcal/{Mcu,Port,Gpt,SysTimer,Adc,Dio,Can}/*_S32K144.c` and
  `platform/s32k144/`: register-level MCAL for the NXP S32K144, selected
  via the new `cmake/s32k144_toolchain.cmake` and
  `-DAUTOSAR_MCAL_VARIANT=s32k144`.
- `tests/unit/{test_bswm.c,test_comm.c,test_cansm.c,test_os_lifecycle.c}`.

### Known deviations from this phase's own `REQ-*` (found by code review, not yet fixed)

Unlike earlier phases, this phase's code does **not** yet satisfy every
requirement it introduces. Listed here rather than silently marked done,
per the project's own documented-deviation convention:

- `REQ-BSWM-001` violated: `BswM_ActionList_Shutdown` never calls
  `Adc_DeInit`/`Dio_DeInit`/`Display_DeInit`, unlike the driver init calls
  its startup counterpart makes.
- `REQ-BSWM-002` violated: `BswM_Init` sets `g_state = BSWM_STATE_SHUTDOWN`
  on every failure branch, so `BswM_DeInit`'s "already shut down" guard
  treats a failed partial init as needing no cleanup.
- `REQ-ECUM-007` violated: `EcuM_StartupTwo` sets
  `g_state = ECUM_STATE_RUN` before `BswM_Init` is even called by its
  caller; nothing reverts it if `BswM_Init` later fails.
- `REQ-ARCH-015` violated: `Os.c`'s `StartOS` reads `BswM_GetState()`
  directly.
- `REQ-ARCH-016` violated: `CMakeLists.txt` still adds
  `sensor_ecu`/`cluster_ecu`/the unit test targets unconditionally, so a
  `-DAUTOSAR_MCAL_VARIANT=s32k144` configure attempts to cross-compile the
  POSIX-dependent test targets.
- `REQ-COMM-001` violated: `ComM_ModeType` and its `COMM_*` literals are
  independently redefined in both `CanSM.h` and `ComM.h` instead of once
  in `ComStack_Types.h`.
- Additional MCAL-level findings outside this phase's own `REQ-*` wording
  but relevant to hardware-port correctness: `Mcu_Hw_InitClocks` and
  `FlexCAN0_Hw_Init` spin on hardware status bits with no timeout after the
  watchdog is already disabled; `Gpt_GetTimeElapsed` discards the
  sub-millisecond tick remainder every call; `Adc_ReadGroup` does not
  validate its `group` argument (unlike the host `Adc.c` and the sibling
  `Dio_S32K144.c`).

Phase 9 exit criteria (spec section 16) are **not** met until these are
fixed and the corresponding tests are extended to cover them.

## Phase 10 — hardware-only pivot (v3.0), structurally done, unverified

Delivered (this restructuring):

- `legacy-sim/`: a full, unmodified copy of the repository as it stood at
  the end of Phase 9, including its own `CMakeLists.txt`,
  `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md`, and the complete 26-test suite —
  standalone and still buildable/testable on Ubuntu independent of
  anything below.
- Removed from the main tree: `bsw/mcal/{Mcu,Port,Gpt,SysTimer,Adc,Dio,
  Can}.c` (the Ubuntu host implementations), `bsw/mcal/Can/Can_Stub.c`,
  the entire `bsw/mcal/VirtualHw/` directory, `bsw/mcal/Display/Display.c`,
  `tools/hmi/`, `tools/{setup_vcan.sh,run_demo.sh,inspect_can.sh}`,
  `scenarios/`, `tests/` (unit, fixtures, and integration all removed),
  and both ECUs' POSIX `main.c` entry points.
- Added: `bsw/mcal/Display/Display_S32K144.c` (new LPUART1 register block
  in `platform/s32k144/S32K144.h`, blocking-TX text output, same
  `Display.h` contract and throttling behavior as the retired host
  driver); `ecu/ClusterEcu/main.c`, a new bare-metal entry point mirroring
  `ecu/SensorEcu/main.c`'s existing pattern (`ClusterEcu` had none
  before).
- Removed the `VirtualHw_ConfigType`/`VirtualHw_Init`/`VirtualHw_DeInit`
  dependency from `BswM_ConfigType`/`EcuM_ConfigType` and `BswM.c`'s
  action lists, and from both ECUs' `*_Cfg.c` — real hardware has no
  "virtual hardware link" to bring up. `Adc`/`Dio`/`Display` config
  structs still carry a vestigial `virtualHwLinkId` field (a plain
  `uint8`); the S32K144 drivers ignore it. Left as-is rather than
  editing the shared `Adc.h`/`Dio.h`/`Display.h` headers, which was out
  of scope for this pass.
- Simplified `CMakeLists.txt`: removed the `AUTOSAR_MCAL_VARIANT`
  real/stub/s32k144 cache-variable switch (only one variant exists now),
  removed every `test_*` target and the integration-test `foreach` loop,
  removed `enable_testing()`, and added a `.hex`/`.bin` post-build step
  per ECU executable via `CMAKE_OBJCOPY`.
- Rewrote `AUTOSAR_CLASSIC_PROJECT_SPEC_v3.0.md`, `README.md`,
  `docs/ARCHITECTURE.md`/`.html`, this file, `docs/REQUIREMENTS_TRACEABILITY.md`,
  and `docs/TEST_PLAN.md` for the hardware-only reality, each pointing at
  `legacy-sim/` for the frozen historical record rather than duplicating
  it.

**Not done / not verified, and explicitly flagged rather than silently
assumed:**

- **No build was run.** No `arm-none-eabi-gcc` toolchain or `cmake` was
  available in the environment this restructuring was done in, so none of
  the new or modified C files (`Display_S32K144.c`,
  `ecu/ClusterEcu/main.c`, the edited `BswM.c`/`BswM.h`/`EcuM.h`/both
  `*_Cfg.c` files, the rewritten `CMakeLists.txt`) has been compiled, let
  alone linked or flashed. Verification was limited to careful manual
  review and repository-wide `grep` for leftover references to removed
  files/symbols (see the commands in the spec's section "Verification").
  **Run the real build before trusting this pivot.**
- No physical hardware bring-up (spec section 14) has been performed.
- The register addresses/pin assignments chosen for the new
  `Display_S32K144.c` (LPUART1 on PTC6/PTC7, PCC index 43, SOSCDIV2
  clock source, OSR=16/SBR=26 for ~9615 baud) are based on the S32K144
  reference manual's documented values and this project's own existing
  `Gpt_S32K144.c`/`Port_S32K144.c` conventions, but have not been checked
  against a datasheet by a second reviewer or verified against real
  silicon. Double-check before relying on them.
- The six Phase 9 issues above are unchanged by this pivot — still open,
  and now more consequential since no test suite remains to catch a
  regression in them.

## Not yet implemented

Phases 0-8 (v1) and the module/driver delivery of Phase 9 (v2.0) are done
against the Ubuntu simulation, frozen in `legacy-sim/`. Phase 9's own
`REQ-*` violations (above) are still open. Phase 10 (v3.0, this
restructuring) is structurally complete but has not been build- or
hardware-verified (above) — do not treat it as working until it has been.
Section 17's Definition of Done is reviewed item by item in
`docs/REQUIREMENTS_TRACEABILITY.md`'s closing note; a small number of DoD
items are met with a documented, explained deviation (the GCC-only build
convention adopted mid-project for the Ubuntu simulation, per user
direction; and the two REQ-ARCH-009 socket-path literals in
`legacy-sim/ecu/<Ecu>/main.c`'s argument defaults) rather than a literal
word-for-word match, and are called out explicitly rather than silently
marked done.
