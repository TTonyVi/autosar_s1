# Test plan

Source of truth for test scope is `AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md`,
section 14. This document tracks what's actually implemented against that
plan; it does not restate the full spec.

## Unit tests (spec section 14.1)

| Test target | Status |
|---|---|
| `test_com_protection` | Not started (Phase 4) |
| `test_com_pack` | Not started (Phase 4) |
| `test_pdur` | Not started (Phase 4) |
| `test_canif` | Not started (Phase 3) |
| `test_rte_sensor` | Not started (Phase 5) |
| `test_rte_cluster` | Not started (Phase 5) |
| `test_cluster_swc` | Not started (Phase 5) |
| `test_virtual_hw` | **Done** — bounded drain, latest wins, malformed/oversized rejected, duplicate/reordered SEQ rejected, send with no peer non-fatal; uses real sockets |
| `test_adc_dio` | **Done** — raw pass-through without scaling, staleness age reporting, unknown channel, default level before first datagram; uses fake VirtualHw/Gpt |
| `test_iohwab` | Not started (Phase 5) |
| `test_display` | **Done** — frame transported verbatim, write failure counted and reported, minimum write period respected, oversized frame rejected; uses fake VirtualHw/Gpt |
| `test_scheduler` | **Done** — due ordering, missed periods, monotonic deadline advancement; deterministic via a fake Gpt |
| `test_det_dem` | **Done** — bounded counters/ring buffer, failed/passed event transitions |
| `test_config` | Not started (Phase 3/4) |
| `test_smoke_phase0` | **Done** — harness smoke test only, see `docs/IMPLEMENTATION_STATUS.md` |

## Integration tests (spec section 14.2)

| ID | Title | Status |
|---|---|---|
| IT-01 | Normal end-to-end communication | Not started (Phase 6) |
| IT-02 | Timeout and recovery | Not started (Phase 6) |
| IT-03 | Integrity rejection | Not started (Phase 8) |
| IT-04 | Layered Tx/Rx evidence | Not started (Phase 8) |
| IT-05 | Virtual hardware input, end to end | Not started (Phase 6) |
| IT-06 | Sensor line disconnected and restored | Not started (Phase 6) |
| IT-07 | Display link absent | Not started (Phase 6) |

## Static checks (spec section 14.3)

| Check | Status |
|---|---|
| GCC + Clang build | **Done through Phase 2 code** — GCC 9.4.0 and Clang 10.0.0 both build cleanly with all mandatory warnings. |
| `clang-format --dry-run --Werror` | **Done through Phase 2 code** — passes on every `.c`/`.h` after `clang-format -i` fixups. |
| Sanitizer integration test | Not started (Phase 8) |
| SW-C forbidden-header check | Not started (Phase 8; nothing in `asw/` to check against yet beyond the Phase 0 placeholders) |
| `REQ-ARCH-009` MCAL isolation grep | **Manually verified through Phase 2** — `clock_gettime`/`sys/socket.h`/`sys/un.h`/`AF_UNIX`/`linux/can.h`/a `"/tmp/` literal all appear only in `bsw/mcal/` or the exempt `tests/`; not yet wired as an automated CI step (Phase 8) |
| `REQ-ARCH-012` stub-MCAL build | Not started (Phase 8) |
| `malloc`/`calloc`/`realloc`/`free` search | **Manually verified through Phase 2** — none found; not yet wired as an automated CI step (Phase 8) |

## How to run what exists today

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
