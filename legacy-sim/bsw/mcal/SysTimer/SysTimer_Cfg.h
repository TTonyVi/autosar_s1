/**
 * @file SysTimer_Cfg.h
 * @brief Static configuration constants for the OS system timer.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 7.4 and 11.2. Kept free of
 * type dependencies so `SysTimer.h` can include it before declaring its own
 * types, matching the usual AUTOSAR `<Module>_Cfg.h` layering.
 */
#ifndef SYSTIMER_CFG_H
#define SYSTIMER_CFG_H

/**
 * Ticks of the OS system timer per millisecond.
 *
 * Deviation from real AUTOSAR, documented deliberately: on a real target
 * this is fixed by the hardware (SysTick reload value on Cortex-M, STM
 * prescaler on AURIX) and is rarely 1. On this Ubuntu host the tick is
 * derived from `CLOCK_MONOTONIC` at exactly 1 tick per millisecond, so the
 * conversion in `Os_Counter_TicksToMs` is the identity here. The conversion
 * still exists as a separate, tested function precisely because it is NOT
 * the identity on a real target — porting changes this constant and
 * nothing else.
 */
#define SYSTIMER_TICKS_PER_MS (1U)

/** The single system-timer channel this project needs. */
#define SYSTIMER_CHANNEL_OS (0U)

#endif /* SYSTIMER_CFG_H */
