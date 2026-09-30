/**
 * @file test_comm.c
 * @brief Unit tests for Communication Manager (bsw/services/ComM).
 */
#include <stdio.h>

#include "CanSM.h"
#include "Com.h"
#include "ComM.h"
#include "Det.h"
#include "Det_ModuleIds.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

/* Stubs for lower-layer calls from ComM */
static ComM_ModeType g_lastCanSMMode = COMM_NO_COMMUNICATION;
Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType ComM_Mode) {
    (void)network;
    g_lastCanSMMode = ComM_Mode;
    return E_OK;
}

static boolean g_ipduGroupStarted;
void Com_IpduGroupStart(Com_IpduGroupIdType group_id, boolean initialize) {
    (void)group_id;
    (void)initialize;
    g_ipduGroupStarted = TRUE;
}

void Com_IpduGroupStop(Com_IpduGroupIdType group_id) {
    (void)group_id;
    g_ipduGroupStarted = FALSE;
}

int main(void) {
    Det_Init();

    /* 1. Null config rejected */
    uint32 beforeErrors = Det_GetTotalErrorCount();
    Std_ReturnType ret = ComM_Init(NULL);
    Check(ret == E_NOT_OK, "ComM_Init(NULL) returns E_NOT_OK");
    Check(Det_GetTotalErrorCount() == beforeErrors + 1U, "null config reports to Det");

    /* 2. Configure 2 users on 1 CAN channel */
    static const ComM_UserHandleType kUsers[2] = {0U, 1U};
    static const ComM_ChannelConfigType kChannels[1] = {
        {.channelId = 0U, .numUsers = 2U, .users = kUsers},
    };
    static const ComM_ConfigType kConfig = {
        .channels = kChannels,
        .numChannels = 1U,
        .numUsers = 2U,
    };

    ret = ComM_Init(&kConfig);
    Check(ret == E_OK, "ComM_Init succeeds with valid config");
    Check(ComM_GetChannelMode(0U) == COMM_NO_COMMUNICATION, "initial channel mode is NO_COMMUNICATION");

    /* 3. User 0 requests FULL_COMMUNICATION */
    ret = ComM_RequestComMode(0U, COMM_FULL_COMMUNICATION);
    Check(ret == E_OK, "ComM_RequestComMode(User 0, FULL_COM) succeeds");
    Check(ComM_GetChannelMode(0U) == COMM_FULL_COMMUNICATION, "channel entered COMM_FULL_COMMUNICATION");
    Check(g_lastCanSMMode == COMM_FULL_COMMUNICATION, "CanSM was requested FULL_COMMUNICATION");
    Check(g_ipduGroupStarted == TRUE, "Com_IpduGroupStart was called");

    /* 4. User 1 also requests FULL_COMMUNICATION */
    ret = ComM_RequestComMode(1U, COMM_FULL_COMMUNICATION);
    Check(ret == E_OK, "User 1 requests FULL_COMMUNICATION");
    Check(ComM_GetChannelMode(0U) == COMM_FULL_COMMUNICATION, "channel stays in FULL_COMMUNICATION");

    /* 5. User 0 releases communication (requests NO_COMMUNICATION) */
    ret = ComM_RequestComMode(0U, COMM_NO_COMMUNICATION);
    Check(ret == E_OK, "User 0 requests NO_COMMUNICATION");
    Check(ComM_GetChannelMode(0U) == COMM_FULL_COMMUNICATION, "channel remains in FULL_COMMUNICATION because User 1 still active");

    /* 6. User 1 also releases communication -> channel drops to NO_COMMUNICATION */
    ret = ComM_RequestComMode(1U, COMM_NO_COMMUNICATION);
    Check(ret == E_OK, "User 1 requests NO_COMMUNICATION");
    Check(ComM_GetChannelMode(0U) == COMM_NO_COMMUNICATION, "channel dropped to COMM_NO_COMMUNICATION after all users released");
    Check(g_lastCanSMMode == COMM_NO_COMMUNICATION, "CanSM was requested NO_COMMUNICATION");
    Check(g_ipduGroupStarted == FALSE, "Com_IpduGroupStop was called");

    /* 7. DeInit */
    ComM_DeInit();

    if (g_failures == 0) {
        (void)printf("test_comm: all checks passed\n");
        return 0;
    }

    (void)fprintf(stderr, "test_comm: %d check(s) failed\n", g_failures);
    return 1;
}
