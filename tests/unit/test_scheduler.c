/**
 * @file test_scheduler.c
 * @brief Phase 1 unit tests for Os (bsw/os/Os).
 *
 * Covers the mandatory coverage from AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md
 * section 14.1 ("test_scheduler"): due ordering including Task_Io_5ms
 * before Task_App_10ms, missed periods, and monotonic deadline
 * advancement. Uses the fake Gpt (tests/fixtures) instead of the real
 * host-clock-based one, so the whole test runs in simulated time and is
 * fully deterministic regardless of host speed.
 */
#include <stdio.h>

#include "Det.h"
#include "Gpt_Fake.h"
#include "Os.h"
#include "Platform_Types.h"

/* Mirrors the real task table from spec section 8.2 (minus the actual BSW
 * callbacks, which are wired starting Phase 3+); indices below identify
 * which task fired in the call log. */
#define TASK_IDX_BSW_1MS 0U
#define TASK_IDX_IO_5MS 1U
#define TASK_IDX_APP_10MS 2U
#define TASK_IDX_BSWCOM_20MS 3U
#define TASK_IDX_APP_100MS 4U
#define NUM_TASKS 5U

#define LOG_CAPACITY 256U
static uint8 g_log[LOG_CAPACITY];
static uint32 g_logCount;

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void LogCall(uint8 taskIndex) {
    if (g_logCount < LOG_CAPACITY) {
        g_log[g_logCount] = taskIndex;
        g_logCount++;
    }
}

static void Task_Bsw_1ms(void) {
    LogCall(TASK_IDX_BSW_1MS);
}
static void Task_Io_5ms(void) {
    LogCall(TASK_IDX_IO_5MS);
}
static void Task_App_10ms(void) {
    LogCall(TASK_IDX_APP_10MS);
}
static void Task_BswCom_20ms(void) {
    LogCall(TASK_IDX_BSWCOM_20MS);
}
static void Task_App_100ms(void) {
    LogCall(TASK_IDX_APP_100MS);
}

static const Os_TaskConfigType g_tasks[NUM_TASKS] = {
    {"Task_Bsw_1ms", 1U, 4U, Task_Bsw_1ms},       {"Task_Io_5ms", 5U, 3U, Task_Io_5ms},
    {"Task_App_10ms", 10U, 2U, Task_App_10ms},    {"Task_BswCom_20ms", 20U, 2U, Task_BswCom_20ms},
    {"Task_App_100ms", 100U, 1U, Task_App_100ms},
};

static const Os_ConfigType g_config = {g_tasks, NUM_TASKS};

static sint32 FindInLog(uint32 from, uint8 taskIndex) {
    for (uint32 i = from; i < g_logCount; i++) {
        if (g_log[i] == taskIndex) {
            return (sint32)i;
        }
    }
    return -1;
}

static void ResetSystem(void) {
    GptFake_Reset();
    Gpt_Init(NULL);
    Det_Init();
    g_logCount = 0U;

    Std_ReturnType r = Os_Init(&g_config);
    Check(r == E_OK, "Os_Init accepts the section 8.2 task table");
}

static void TestDueOrdering(void) {
    ResetSystem();

    /* Step the fake clock 1ms at a time up to 20ms, exactly like a real
     * main loop calling Os_MainFunction every tick. At t=10ms, both
     * Task_Io_5ms (period 5, priority 3) and Task_App_10ms (period 10,
     * priority 2) become due in the very same MainFunction call. */
    for (uint32 t = 1U; t <= 20U; t++) {
        GptFake_SetTimeMs(t);
        Os_MainFunction();
    }

    sint32 appPos = FindInLog(0U, TASK_IDX_APP_10MS);
    Check(appPos > 0, "Task_App_10ms fired by t=20ms");
    if (appPos > 0) {
        /* Nothing has a priority strictly between Io_5ms's 3 and
         * App_10ms's 2, so when both fire in the same tick, Io_5ms is
         * always logged immediately before App_10ms. */
        Check(g_log[appPos - 1] == TASK_IDX_IO_5MS,
              "Task_Io_5ms (priority 3) runs immediately before Task_App_10ms (priority 2) when "
              "both are due together");
    }
}

static void TestMissedPeriods(void) {
    ResetSystem();

    /* Never poll between 0 and 500ms: Task_App_100ms (period 100, first
     * release at 100ms) has releases due at 100,200,300,400,500 by the
     * time we finally check — one execution now, four recorded as
     * missed. */
    GptFake_SetTimeMs(500U);
    Os_MainFunction();

    Os_TaskStatsType stats;
    Std_ReturnType r = Os_GetTaskStats(TASK_IDX_APP_100MS, &stats);
    Check(r == E_OK, "Os_GetTaskStats succeeds for Task_App_100ms");
    Check(stats.invocationCount == 1U,
          "a starved task still executes exactly once per MainFunction call");
    Check(stats.missedCount == 4U,
          "4 missed periods recorded (releases due at 100,200,300,400 were skipped)");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_OS) > 0U,
          "a missed-period overrun is reported to Det (section 10.1)");
}

static void TestMonotonicDeadlineAdvancement(void) {
    ResetSystem();

    Os_TaskStatsType stats;

    GptFake_SetTimeMs(1U);
    Os_MainFunction();
    (void)Os_GetTaskStats(TASK_IDX_BSW_1MS, &stats);
    Check(stats.nextReleaseMs == 2U,
          "after the 1ms task's first release, its next release is exactly one period later");

    /* The release schedule advances by fixed absolute steps, never by
     * "now + period" (which would drift under variable call timing). */
    GptFake_SetTimeMs(2U);
    Os_MainFunction();
    (void)Os_GetTaskStats(TASK_IDX_BSW_1MS, &stats);
    Check(stats.nextReleaseMs == 3U,
          "next release advances by exactly one period, with no accumulated drift");
}

static void TestInvalidConfigRejected(void) {
    GptFake_Reset();
    Gpt_Init(NULL);
    Det_Init();

    uint32 detBefore;
    Std_ReturnType r;

    r = Os_Init(NULL);
    Check(r == E_NOT_OK, "Os_Init rejects a null config pointer (REQ-CFG-001)");

    static const Os_TaskConfigType emptyTasks[1] = {{"unused", 1U, 1U, Task_Bsw_1ms}};
    const Os_ConfigType zeroTaskConfig = {emptyTasks, 0U};
    r = Os_Init(&zeroTaskConfig);
    Check(r == E_NOT_OK, "Os_Init rejects a config with zero tasks");

    const Os_ConfigType tooManyTasksConfig = {emptyTasks, (uint8)(OS_MAX_TASKS + 1U)};
    r = Os_Init(&tooManyTasksConfig);
    Check(r == E_NOT_OK, "Os_Init rejects a task count above OS_MAX_TASKS");

    static const Os_TaskConfigType badCallback[1] = {{"bad", 1U, 1U, NULL}};
    const Os_ConfigType badCallbackConfig = {badCallback, 1U};
    detBefore = Det_GetTotalErrorCount();
    r = Os_Init(&badCallbackConfig);
    Check(r == E_NOT_OK, "Os_Init rejects a task with a null callback");
    Check(Det_GetTotalErrorCount() > detBefore, "an invalid Os config is reported to Det");

    static const Os_TaskConfigType zeroPeriod[1] = {{"bad", 0U, 1U, Task_Bsw_1ms}};
    const Os_ConfigType zeroPeriodConfig = {zeroPeriod, 1U};
    r = Os_Init(&zeroPeriodConfig);
    Check(r == E_NOT_OK, "Os_Init rejects a task with a zero period");

    /* A call made before a successful Init is rejected, not crashed
     * (REQ-ECUM-002: "A module SHALL reject service calls made before its
     * initialization"). */
    Os_DeInit();
    Os_MainFunction(); /* must not crash */
    Os_TaskStatsType stats;
    r = Os_GetTaskStats(0U, &stats);
    Check(r == E_NOT_OK, "Os_GetTaskStats rejects a call before Os_Init (REQ-ECUM-002)");
}

int main(void) {
    TestDueOrdering();
    TestMissedPeriods();
    TestMonotonicDeadlineAdvancement();
    TestInvalidConfigRejected();

    if (g_failures == 0) {
        (void)printf("test_scheduler: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_scheduler: %d check(s) failed\n", g_failures);
    return 1;
}
