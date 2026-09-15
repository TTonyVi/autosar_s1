/**
 * @file test_systimer.c
 * @brief Unit tests for SysTimer (bsw/mcal/SysTimer).
 *
 * Exercises the REAL SysTimer.c against the real host clock — this is the
 * module that owns `clock_gettime` for the OS, so like test_virtual_hw and
 * test_can it must be tested against the real thing, not a double. What is
 * asserted is only what is deterministic: initialization contract,
 * monotonicity, and Det reporting. No assertion depends on host speed.
 *
 * Covers spec section 14.1 row "test_systimer".
 */
#include <stdio.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "Platform_Types.h"
#include "SysTimer.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void TestReadBeforeInitIsRejected(void) {
    Det_Init();
    SysTimer_DeInit(); /* guarantee the uninitialized state */

    uint32 before = Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER);
    SysTimer_TickType ticks = SysTimer_GetTicks();

    Check(ticks == 0U, "SysTimer_GetTicks before init returns 0");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER) == before + 1U,
          "SysTimer_GetTicks before init reports one Det error");
}

static void TestInitStartsTheTimeBase(void) {
    Det_Init();
    SysTimer_Init(&SysTimer_Config);

    uint32 errs = Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER);
    Check(errs == 0U, "a valid config produces no Det error");

    /* Ticks immediately after init must be small but are allowed to be
     * non-zero: reading the clock itself takes time. The upper bound is
     * generous so a loaded CI host cannot make this flaky. */
    SysTimer_TickType ticks = SysTimer_GetTicks();
    Check(ticks < 1000U, "ticks just after init are near zero");
}

static void TestTicksAreMonotonic(void) {
    Det_Init();
    SysTimer_Init(&SysTimer_Config);

    SysTimer_TickType previous = SysTimer_GetTicks();
    boolean monotonic = TRUE;
    for (uint32 i = 0U; i < 2000U; i++) {
        SysTimer_TickType current = SysTimer_GetTicks();
        if (current < previous) {
            monotonic = FALSE;
        }
        previous = current;
    }
    Check(monotonic == TRUE, "ticks never decrease across repeated reads");
}

static void TestTicksAdvanceWithRealTime(void) {
    Det_Init();
    SysTimer_Init(&SysTimer_Config);

    SysTimer_TickType start = SysTimer_GetTicks();
    SysTimer_TickType now = start;
    /* Busy-wait rather than sleep: sleeping would need another POSIX
     * feature-test macro in a file that is not allowed to own the clock.
     * The cap bounds the test at well under a second on any host. */
    for (uint32 i = 0U; (i < 50000000U) && (now == start); i++) {
        now = SysTimer_GetTicks();
    }
    Check(now > start, "ticks advance as real time passes");
}

static void TestNullConfigIsReportedButStillStarts(void) {
    Det_Init();
    SysTimer_DeInit();

    SysTimer_Init(NULL);

    Check(Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER) == 1U,
          "a null config reports exactly one Det error");
    /* Still started: a stopped OS time base would stop the scheduler. */
    uint32 before = Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER);
    (void)SysTimer_GetTicks();
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER) == before,
          "the time base still runs after a null config");
}

static void TestInconsistentConfigIsReportedButStillStarts(void) {
    Det_Init();
    SysTimer_DeInit();

    const SysTimer_ConfigType bad = {
        .channelId = (uint8)(SYSTIMER_CHANNEL_OS + 1U),
        .ticksPerMs = SYSTIMER_TICKS_PER_MS,
    };
    SysTimer_Init(&bad);

    Check(Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER) == 1U,
          "a wrong channel id reports exactly one Det error");
    uint32 before = Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER);
    (void)SysTimer_GetTicks();
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER) == before,
          "the time base still runs after an inconsistent config");
}

static void TestDeInitStopsTheTimeBase(void) {
    Det_Init();
    SysTimer_Init(&SysTimer_Config);
    SysTimer_DeInit();

    uint32 before = Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER);
    Check(SysTimer_GetTicks() == 0U, "reads after DeInit return 0");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_SYSTIMER) == before + 1U,
          "reads after DeInit report a Det error");
}

int main(void) {
    TestReadBeforeInitIsRejected();
    TestInitStartsTheTimeBase();
    TestTicksAreMonotonic();
    TestTicksAdvanceWithRealTime();
    TestNullConfigIsReportedButStillStarts();
    TestInconsistentConfigIsReportedButStillStarts();
    TestDeInitStopsTheTimeBase();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_systimer: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_systimer: all checks passed\n");
    return 0;
}
