/**
 * @file test_os_counter.c
 * @brief Unit tests for Os_Counter (bsw/os/Os/Os_Counter.c).
 *
 * Exercises the REAL Os_Counter against the FAKE SysTimer
 * (tests/fixtures/SysTimer_Fake.c), so every tick value is set by hand and
 * nothing depends on host speed.
 *
 * The point of this layer is that the OS reads its own counter, never the
 * general-purpose timer (spec section 8.1) — that separation is enforced
 * by the static check in section 14.3, and the behavioural contract
 * (zeroing at init, tick-to-millisecond conversion, wrap-around) is what
 * is asserted here.
 *
 * Covers spec section 14.1 row "test_os_counter".
 */
#include <stdio.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "Os_Counter.h"
#include "Platform_Types.h"
#include "SysTimer_Fake.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void SetUp(void) {
    Det_Init();
    SysTimerFake_Reset();
    SysTimer_Init(&SysTimer_Config);
}

static void TestCounterStartsAtZero(void) {
    SetUp();
    SysTimerFake_SetTicks(0U);
    Os_Counter_Init();

    Check(Os_Counter_GetValue() == 0U, "the counter reads 0 right after init");
}

static void TestCounterIsZeroedAgainstTheTickAtInit(void) {
    SetUp();
    /* SysTimer has already been running for a while when the OS starts —
     * the OS counter must still begin at 0 (OSEK counter semantics). */
    SysTimerFake_SetTicks(5000U);
    Os_Counter_Init();

    Check(Os_Counter_GetValue() == 0U, "init zeroes the counter against the current tick");

    SysTimerFake_AdvanceTicks(7U);
    Check(Os_Counter_GetValue() == 7U,
          "the counter counts from its own origin, not from SysTimer 0");
}

static void TestCounterFollowsTheTimer(void) {
    SetUp();
    Os_Counter_Init();

    SysTimerFake_SetTicks(1U);
    Check(Os_Counter_GetValue() == 1U, "counter follows a single tick");

    SysTimerFake_SetTicks(1234U);
    Check(Os_Counter_GetValue() == 1234U, "counter follows a large jump");

    SysTimerFake_AdvanceTicks(66U);
    Check(Os_Counter_GetValue() == 1300U, "counter follows a relative advance");
}

static void TestTicksToMsConversion(void) {
    /* SYSTIMER_TICKS_PER_MS is 1 on this host, so the conversion is the
     * identity here. Asserting it explicitly is what makes a future change
     * of the tick rate a test failure rather than a silent timing bug. */
    Check(Os_Counter_TicksToMs(0U) == 0U, "0 ticks is 0 ms");
    Check(Os_Counter_TicksToMs(1U) == (Os_MilliType)(1U / SYSTIMER_TICKS_PER_MS),
          "1 tick converts per the configured rate");
    Check(Os_Counter_TicksToMs(1000U) == (Os_MilliType)(1000U / SYSTIMER_TICKS_PER_MS),
          "1000 ticks convert per the configured rate");
    Check(Os_Counter_TicksToMs(4294967295U) == (Os_MilliType)(4294967295U / SYSTIMER_TICKS_PER_MS),
          "the maximum tick value converts without overflow");
}

static void TestConversionIsMonotonic(void) {
    boolean monotonic = TRUE;
    Os_MilliType previous = Os_Counter_TicksToMs(0U);
    for (Os_TickType t = 1U; t < 5000U; t++) {
        Os_MilliType current = Os_Counter_TicksToMs(t);
        if (current < previous) {
            monotonic = FALSE;
        }
        previous = current;
    }
    Check(monotonic == TRUE, "tick-to-millisecond conversion never goes backwards");
}

static void TestReadBeforeInitIsRejected(void) {
    Det_Init();
    SysTimerFake_Reset();
    SysTimer_Init(&SysTimer_Config);
    Os_Counter_DeInit(); /* guarantee the uninitialized state */

    uint32 before = Det_GetModuleErrorCount(DET_MODULE_ID_OS);
    Check(Os_Counter_GetValue() == 0U, "reading an uninitialized counter returns 0");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_OS) == before + 1U,
          "reading an uninitialized counter reports a Det error");
}

static void TestWrapAroundIsHandled(void) {
    SetUp();
    /* Start the counter close to the 32-bit tick rollover. */
    SysTimerFake_SetTicks(4294967290U); /* 2^32 - 6 */
    Os_Counter_Init();
    Check(Os_Counter_GetValue() == 0U, "counter starts at 0 near the rollover");

    SysTimerFake_AdvanceTicks(10U); /* wraps past 2^32 */
    Check(Os_Counter_GetValue() == 10U, "counter stays correct across the tick rollover");
}

int main(void) {
    TestCounterStartsAtZero();
    TestCounterIsZeroedAgainstTheTickAtInit();
    TestCounterFollowsTheTimer();
    TestTicksToMsConversion();
    TestConversionIsMonotonic();
    TestReadBeforeInitIsRejected();
    TestWrapAroundIsHandled();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_os_counter: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_os_counter: all checks passed\n");
    return 0;
}
