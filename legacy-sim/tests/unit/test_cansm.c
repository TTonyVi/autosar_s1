/**
 * @file test_cansm.c
 * @brief Unit tests for CAN State Manager (bsw/communication/CanSM).
 */
#include <stdio.h>

#include "CanIf.h"
#include "CanSM.h"
#include "Det.h"
#include "Det_ModuleIds.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static Can_ControllerStateType g_lastControllerMode = CAN_CS_UNINIT;
Std_ReturnType CanIf_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode) {
    (void)controller_id;
    g_lastControllerMode = mode;
    return E_OK;
}

int main(void) {
    Det_Init();

    /* 1. Initial state */
    Check(CanSM_GetState(0U) == CANSM_UNINIT, "initial state is CANSM_UNINIT");

    /* 2. Null config rejected */
    uint32 beforeErrors = Det_GetTotalErrorCount();
    CanSM_Init(NULL);
    Check(Det_GetTotalErrorCount() == beforeErrors + 1U, "null config reports to Det");

    /* 3. Valid init */
    static const CanSM_NetworkConfigType kNetworks[1] = {{.canControllerId = 0U, .networkHandle = 0U}};
    static const CanSM_ConfigType kConfig = {.networks = kNetworks, .numNetworks = 1U};
    CanSM_Init(&kConfig);
    Check(CanSM_GetState(0U) == CANSM_NOCOM, "state after init is CANSM_NOCOM");

    /* 4. Request FULL_COMMUNICATION */
    Std_ReturnType r = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);
    Check(r == E_OK, "CanSM_RequestComMode FULL_COMMUNICATION succeeds");
    Check(CanSM_GetState(0U) == CANSM_FULLCOM, "state transitioned to CANSM_FULLCOM");
    Check(g_lastControllerMode == CAN_CS_STARTED, "CanIf_SetControllerMode called with CAN_CS_STARTED");

    ComM_ModeType curMode = COMM_NO_COMMUNICATION;
    r = CanSM_GetCurrentComMode(0U, &curMode);
    Check(r == E_OK && curMode == COMM_FULL_COMMUNICATION, "CanSM_GetCurrentComMode returns FULL_COMMUNICATION");

    /* 5. Request NO_COMMUNICATION */
    r = CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION);
    Check(r == E_OK, "CanSM_RequestComMode NO_COMMUNICATION succeeds");
    Check(CanSM_GetState(0U) == CANSM_NOCOM, "state transitioned to CANSM_NOCOM");
    Check(g_lastControllerMode == CAN_CS_STOPPED, "CanIf_SetControllerMode called with CAN_CS_STOPPED");

    /* 6. DeInit */
    CanSM_DeInit();
    Check(CanSM_GetState(0U) == CANSM_UNINIT, "state after deinit is CANSM_UNINIT");

    if (g_failures == 0) {
        (void)printf("test_cansm: all checks passed\n");
        return 0;
    }

    (void)fprintf(stderr, "test_cansm: %d check(s) failed\n", g_failures);
    return 1;
}
