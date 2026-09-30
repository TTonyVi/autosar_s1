/**
 * @file SysTimer_Fake.h
 * @brief Test double for SysTimer (bsw/mcal/SysTimer), unit tests only.
 *
 * Implements the exact public SysTimer.h API (SysTimer_Init,
 * SysTimer_DeInit, SysTimer_GetTicks) backed by a manually-set tick count
 * instead of the host clock, so `Os_Counter` and the scheduler run in
 * simulated time and are deterministic regardless of host speed.
 *
 * It does NOT define `SysTimer_Config`: tests link the real
 * `bsw/mcal/SysTimer/SysTimer_Cfg.c` for that, since the config is pure
 * const data with no clock dependency.
 *
 * Per spec section 12.1 this file lives under tests/ and is never linked
 * into a production ECU target — test executables link this INSTEAD OF
 * SysTimer.c, never alongside it.
 */
#ifndef SYSTIMER_FAKE_H
#define SYSTIMER_FAKE_H

#include "SysTimer.h" /* the API this fake substitutes for testing */

/** Resets the fake tick counter to 0 and marks the timer uninitialized. */
void SysTimerFake_Reset(void);

/** Sets the fake tick counter to an absolute value. */
void SysTimerFake_SetTicks(SysTimer_TickType ticks);

/** Advances the fake tick counter by a relative amount. */
void SysTimerFake_AdvanceTicks(SysTimer_TickType deltaTicks);

#endif /* SYSTIMER_FAKE_H */
