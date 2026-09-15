/**
 * @file Os_Counter.h
 * @brief OS counter layer (OSEK counter concept, simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 8.1.
 *
 * In OSEK/AUTOSAR OS a *counter* is the OS-owned abstraction over the
 * hardware tick source that alarms and the schedule table are driven from.
 * This module is that layer: it reads the dedicated `SysTimer` driver and
 * converts ticks to milliseconds for the scheduler.
 *
 * Deliberately narrow contract:
 * - It reads `SysTimer` and NOTHING else. It never calls the
 *   general-purpose timer (`Gpt`) — spec section 14.3 enforces that with a
 *   static check — and never reads the host clock API directly
 *   (REQ-ARCH-009 — only `bsw/mcal/` may).
 * - This header declares its own scalar types rather than including any
 *   MCAL header, so `Os.h` stays free of MCAL dependencies.
 */
#ifndef OS_COUNTER_H
#define OS_COUNTER_H

#include "Platform_Types.h"
#include "Std_Types.h"

/** Counter value in OS ticks, zeroed at `Os_Counter_Init`. */
typedef uint32 Os_TickType;

/** A duration or timestamp in milliseconds, derived from Os_TickType. */
typedef uint32 Os_MilliType;

/**
 * Zeroes the OS counter against the current `SysTimer` tick. Requires
 * `SysTimer_Init` to have run first (spec section 9.2 init order).
 */
void Os_Counter_Init(void);

/**
 * Returns the counter to the uninitialized state. Required so EcuM's
 * shutdown can tear modules down in exactly the reverse of the init order
 * (spec section 9.3), and so a read after shutdown is a reportable error
 * rather than a stale value.
 */
void Os_Counter_DeInit(void);

/**
 * Ticks elapsed since `Os_Counter_Init`. Returns 0 and reports a Det error
 * if called before initialization.
 */
Os_TickType Os_Counter_GetValue(void);

/** Converts OS ticks to milliseconds using the configured tick rate. */
Os_MilliType Os_Counter_TicksToMs(Os_TickType ticks);

#endif /* OS_COUNTER_H */
