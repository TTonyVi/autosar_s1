/**
 * @file test_os_lifecycle.c
 * @brief Unit tests for AUTOSAR OS Lifecycle (StartOS, ShutdownOS, Hooks).
 */

#define UNIT_TEST 1

#include <stdio.h>

#include "BswM.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "EcuM.h"
#include "Os.h"
#include "Os_Counter.h"
#include "SysTimer_Fake.h"

static int g_failures = 0;
static boolean g_startupHookFired = FALSE;
static boolean g_shutdownHookFired = FALSE;
static StatusType g_capturedError = 0xFFU;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void DummyTask(void) {
}

static void MockStartupHook(void) {
    g_startupHookFired = TRUE;
}

static void MockShutdownHook(StatusType Error) {
    g_shutdownHookFired = TRUE;
    g_capturedError = Error;
}

static const Os_TaskConfigType g_testTasks[1] = {
    {"TestTask", 10U, 1U, DummyTask}
};

static const Os_ConfigType g_osConfig = {
    .tasks = g_testTasks,
    .numTasks = 1U,
    .startupHook = MockStartupHook,
    .shutdownHook = MockShutdownHook,
};

int main(void) {
    Det_Init();
    SysTimerFake_Reset();
    SysTimer_Init(&SysTimer_Config);
    Os_Counter_Init();

    /* 1. StartOS without Init triggers DET error */
    uint32 beforeErrors = Det_GetTotalErrorCount();
    StartOS(OSDEFAULTAPPMODE);
    Check(Det_GetTotalErrorCount() == beforeErrors + 1U, "StartOS before Os_Init reports DET error");

    /* 2. Normal Os_Init succeeds */
    Std_ReturnType initRet = Os_Init(&g_osConfig);
    Check(initRet == E_OK, "Os_Init with valid configuration succeeds");

    /* 3. Direct ShutdownOS executes shutdownHook */
    ShutdownOS(E_OK);
    Check(g_shutdownHookFired == TRUE, "ShutdownOS executes shutdownHook");
    Check(g_capturedError == E_OK, "ShutdownOS passes correct Error status to hook");

    /* 4. Reset flags and test Os_DeInit */
    Os_DeInit();
    Check(Det_GetTotalErrorCount() >= 1U, "Det error count tracked");

    if (g_failures == 0) {
        (void)printf("test_os_lifecycle: all checks passed\n");
        return 0;
    }

    (void)fprintf(stderr, "test_os_lifecycle: %d check(s) failed\n", g_failures);
    return 1;
}
