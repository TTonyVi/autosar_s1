#include "Can_Fake.h"

#include <stddef.h>
#include <string.h>

/* Mirrors Can.c's real CAN_MAX_CONTROLLERS; not shared via a header since
 * that constant is private to Can.c and no test needs it to match
 * exactly -- only the two controller IDs this project actually uses (0)
 * need to fit. */
#define CAN_FAKE_MAX_CONTROLLERS ((uint8)2U)

typedef struct {
    boolean configured;
    Can_ControllerStateType mode;
} CanFake_ControllerStateType;

static CanFake_ControllerStateType g_controllers[CAN_FAKE_MAX_CONTROLLERS];
static Std_ReturnType g_setControllerModeResult;
static Std_ReturnType g_writeResult;
static uint32 g_writeCount;
static boolean g_hasLastWrite;
static CanFake_WriteRecordType g_lastWrite;

void Can_Init(const Can_ConfigType *config) {
    (void)memset(g_controllers, 0, sizeof(g_controllers));
    if ((config != NULL) && (config->controllers != NULL)) {
        for (uint8 i = 0U; i < config->numControllers; i++) {
            uint8 id = config->controllers[i].controllerId;
            if (id < CAN_FAKE_MAX_CONTROLLERS) {
                g_controllers[id].configured = TRUE;
                g_controllers[id].mode = CAN_CS_STOPPED;
            }
        }
    }
}

void Can_DeInit(void) {
    (void)memset(g_controllers, 0, sizeof(g_controllers));
}

Std_ReturnType Can_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode) {
    if (controller_id >= CAN_FAKE_MAX_CONTROLLERS) {
        return E_NOT_OK;
    }
    if (g_setControllerModeResult == E_OK) {
        g_controllers[controller_id].mode = mode;
    }
    return g_setControllerModeResult;
}

Can_ControllerStateType Can_GetControllerMode(uint8 controller_id) {
    if (controller_id >= CAN_FAKE_MAX_CONTROLLERS) {
        return CAN_CS_UNINIT;
    }
    return g_controllers[controller_id].mode;
}

boolean Can_IsControllerReady(uint8 controller_id) {
    if (controller_id >= CAN_FAKE_MAX_CONTROLLERS) {
        return FALSE;
    }
    return g_controllers[controller_id].configured;
}

Std_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu_info) {
    g_writeCount++;
    if (pdu_info != NULL) {
        g_lastWrite.hth = hth;
        g_lastWrite.swPduHandle = pdu_info->swPduHandle;
        g_lastWrite.length = pdu_info->length;
        g_lastWrite.id = pdu_info->id;
        (void)memset(g_lastWrite.data, 0, sizeof(g_lastWrite.data));
        uint8 copyLen = (pdu_info->length <= 8U) ? pdu_info->length : 8U;
        if ((pdu_info->sdu != NULL) && (copyLen > 0U)) {
            (void)memcpy(g_lastWrite.data, pdu_info->sdu, copyLen);
        }
        g_hasLastWrite = TRUE;
    }
    return g_writeResult;
}

void Can_MainFunction_Read(void) {
    /* The fake has no real socket to drain; test_canif exercises
     * CanIf_RxIndication directly instead. */
}

void Can_MainFunction_Write(void) {
    /* Reserved in the real Can, no-op here too. */
}

uint32 Can_GetRxFrameCount(uint8 controller_id) {
    (void)controller_id;
    return 0U;
}

uint32 Can_GetRxDroppedCount(uint8 controller_id) {
    (void)controller_id;
    return 0U;
}

uint32 Can_GetTxFailureCount(uint8 controller_id) {
    (void)controller_id;
    return 0U;
}

void CanFake_Reset(void) {
    (void)memset(g_controllers, 0, sizeof(g_controllers));
    g_setControllerModeResult = E_OK;
    g_writeResult = E_OK;
    g_writeCount = 0U;
    g_hasLastWrite = FALSE;
    (void)memset(&g_lastWrite, 0, sizeof(g_lastWrite));
}

void CanFake_SetControllerModeDirect(uint8 controller_id, Can_ControllerStateType mode) {
    if (controller_id < CAN_FAKE_MAX_CONTROLLERS) {
        g_controllers[controller_id].configured = TRUE;
        g_controllers[controller_id].mode = mode;
    }
}

void CanFake_SetSetControllerModeResult(Std_ReturnType result) {
    g_setControllerModeResult = result;
}

void CanFake_SetWriteResult(Std_ReturnType result) {
    g_writeResult = result;
}

uint32 CanFake_GetWriteCount(void) {
    return g_writeCount;
}

boolean CanFake_GetLastWrite(CanFake_WriteRecordType *out) {
    if ((out == NULL) || (!g_hasLastWrite)) {
        return FALSE;
    }
    *out = g_lastWrite;
    return TRUE;
}
