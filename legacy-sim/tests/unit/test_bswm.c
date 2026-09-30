/**
 * @file test_bswm.c
 * @brief Unit tests for BSW Mode Manager (bsw/services/BswM).
 */
#include <stdio.h>

#include "BswM.h"
#include "Det.h"
#include "Det_ModuleIds.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

/* Stubs for RTE symbols used by BswM */
Std_ReturnType Rte_Start(void) {
    return E_OK;
}
Std_ReturnType Rte_Stop(void) {
    return E_OK;
}

int main(void) {
    Det_Init();

    /* 1. Initial state */
    Check(BswM_GetState() == BSWM_STATE_UNINIT, "initial state is BSWM_STATE_UNINIT");

    /* 2. Null config rejected */
    uint32 beforeErrors = Det_GetTotalErrorCount();
    Std_ReturnType ret = BswM_Init(NULL);
    Check(ret == E_NOT_OK, "BswM_Init(NULL) returns E_NOT_OK");
    Check(BswM_GetState() == BSWM_STATE_SHUTDOWN, "state transitions to BSWM_STATE_SHUTDOWN on error");
    Check(Det_GetTotalErrorCount() == beforeErrors + 1U, "null config reports to Det");

    /* 3. DeInit is safe to call when SHUTDOWN / UNINIT */
    BswM_DeInit();
    Check(BswM_GetState() == BSWM_STATE_UNINIT, "BswM_DeInit transitions to BSWM_STATE_UNINIT");
    BswM_DeInit(); /* safe to call again */

    if (g_failures == 0) {
        (void)printf("test_bswm: all checks passed\n");
        return 0;
    }

    (void)fprintf(stderr, "test_bswm: %d check(s) failed\n", g_failures);
    return 1;
}
