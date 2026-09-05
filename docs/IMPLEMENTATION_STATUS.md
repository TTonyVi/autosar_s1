# Implementation status

Tracks progress against the phase plan in `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md`,
section 16. Updated at the end of every phase, per the spec's own
instruction (section 0.2, item 4).

| Phase | Title | Status | Notes |
|---:|---|---|---|
| 0 | Repository and documentation baseline | **Done** | See below. |
| 1 | Base MCAL, services, time, and scheduler | **Done** | See below. |
| 2 | Virtual hardware links and input/output MCAL | **Done** | See below. |
| 3 | Can Driver and CanIf | **Done** | See below. |
| 4 | PduR and COM | **Done** | See below. |
| 5 | IoHwAb, RTE, and SW-Cs | Not started | |
| 6 | EcuM and complete ECU integration | Not started | |
| 7 | Tkinter HMI and scenario player | Not started | |
| 8 | Robustness and handoff | Not started | |

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

## Not yet implemented

`IoHwAb`, RTE APIs, both SW-Cs (Phase 5); `EcuM` and the real ECU
main-loop wiring, including the `Os_MainFunction` outer loop with
`clock_nanosleep` (Phase 6); the Tkinter HMI and scenario player
(Phase 7); sanitizers, remaining static checks, and full traceability
(Phase 8).
