/**
 * @file Os.h
 * @brief Cooperative cyclic-executive scheduler (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 8. Each ECU is a
 * single-threaded process; this is a simplified host realization of
 * task/runnable mapping, not a full AUTOSAR OS. All timing is derived from
 * `Os_Counter` — the OS-owned counter over the dedicated `SysTimer`
 * driver — never from `Gpt` and never from a direct clock read
 * (section 8.1). `Gpt` serves the other BSW modules only.
 *
 * Design notes for Phase 1 (see docs/IMPLEMENTATION_STATUS.md):
 * - `Os_MainFunction` performs one scheduling pass: it does not sleep and
 *   does not loop by itself. The real-time outer loop (call
 *   `Os_MainFunction` repeatedly, optionally `clock_nanosleep` between
 *   calls per section 8.1's "MAY sleep") is wired into each ECU's main.c
 *   (under ecu/) in Phase 6. This keeps the scheduler itself pure logic, fully
 *   deterministic and host-independent, and testable with a fake
 *   `SysTimer` without any real time passing.
 * - Priority convention: a LARGER numeric `priority` value runs first
 *   when multiple tasks are due at the same check (REQ: "execute higher
 *   priority first"). This matches section 8.2's table, where the most
 *   time-critical 1ms bus task has priority 4 and the least time-critical
 *   100ms application task has priority 1. Ties (equal priority) are
 *   broken by position in the `tasks` config array — earlier wins; this
 *   tie-break is a project-local convention, not specified by the spec.
 */
#ifndef OS_H
#define OS_H

#include "Os_Counter.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef void (*Os_TaskFuncType)(void);

typedef struct {
    const char *name; /* for logging only; never compared/switched on */
    uint32 periodMs;
    uint8 priority; /* larger value = higher priority; see file header */
    Os_TaskFuncType callback;
} Os_TaskConfigType;

typedef struct {
    const Os_TaskConfigType *tasks;
    uint8 numTasks;
} Os_ConfigType;

typedef struct {
    uint32 invocationCount;
    uint32 maxStartLatencyMs;
    uint32 maxExecTimeMs;
    uint32 missedCount;
    Os_MilliType nextReleaseMs;
} Os_TaskStatsType;

/** Fixed upper bound on the number of configured tasks (spec section 8.2
 *  configures exactly 5 per ECU); bounded, no dynamic allocation. */
#define OS_MAX_TASKS ((uint8)8U)

Std_ReturnType Os_Init(const Os_ConfigType *config);
void Os_DeInit(void);

/** Runs exactly one scheduling pass: checks all tasks against the current
 *  Os_Counter time, executes each due task once (in priority order), and updates
 *  per-task statistics. Never blocks, never sleeps. */
void Os_MainFunction(void);

Std_ReturnType Os_GetTaskStats(uint8 taskIndex, Os_TaskStatsType *out);

#endif /* OS_H */
