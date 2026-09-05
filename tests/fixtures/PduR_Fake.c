#include "PduR_Fake.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    Std_ReturnType comTransmitResult;
    uint32 comTransmitCount;
    boolean hasLastComTransmit;
    PduIdType lastComTransmitId;
    uint8 lastComTransmitData[8];
    uint8 lastComTransmitLength;

    uint32 canIfRxIndicationCount;
    boolean hasLastCanIfRxIndicationId;
    PduIdType lastCanIfRxIndicationId;

    uint32 canIfTxConfirmationCount;
    boolean hasLastCanIfTxConfirmationId;
    PduIdType lastCanIfTxConfirmationId;
} PduRFake_StateType;

static PduRFake_StateType g_fake;

Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *info) {
    g_fake.comTransmitCount++;
    if (info != NULL) {
        g_fake.lastComTransmitId = id;
        uint8 len = (info->SduLength <= 8U) ? (uint8)info->SduLength : 8U;
        (void)memset(g_fake.lastComTransmitData, 0, sizeof(g_fake.lastComTransmitData));
        if ((info->SduDataPtr != NULL) && (len > 0U)) {
            (void)memcpy(g_fake.lastComTransmitData, info->SduDataPtr, len);
        }
        g_fake.lastComTransmitLength = len;
        g_fake.hasLastComTransmit = TRUE;
    }
    return g_fake.comTransmitResult;
}

void PduR_CanIfRxIndication(PduIdType id, const PduInfoType *info) {
    (void)info;
    g_fake.canIfRxIndicationCount++;
    g_fake.lastCanIfRxIndicationId = id;
    g_fake.hasLastCanIfRxIndicationId = TRUE;
}

void PduR_CanIfTxConfirmation(PduIdType id) {
    g_fake.canIfTxConfirmationCount++;
    g_fake.lastCanIfTxConfirmationId = id;
    g_fake.hasLastCanIfTxConfirmationId = TRUE;
}

void PduRFake_Reset(void) {
    (void)memset(&g_fake, 0, sizeof(g_fake));
    g_fake.comTransmitResult = E_OK;
}

void PduRFake_SetComTransmitResult(Std_ReturnType result) {
    g_fake.comTransmitResult = result;
}

uint32 PduRFake_GetComTransmitCount(void) {
    return g_fake.comTransmitCount;
}

boolean PduRFake_GetLastComTransmit(PduIdType *idOut, uint8 *dataOut, uint8 bufferSize,
                                    uint8 *lengthOut) {
    if (!g_fake.hasLastComTransmit) {
        return FALSE;
    }
    if (idOut != NULL) {
        *idOut = g_fake.lastComTransmitId;
    }
    uint8 copyLen =
        (g_fake.lastComTransmitLength <= bufferSize) ? g_fake.lastComTransmitLength : bufferSize;
    if ((dataOut != NULL) && (copyLen > 0U)) {
        (void)memcpy(dataOut, g_fake.lastComTransmitData, copyLen);
    }
    if (lengthOut != NULL) {
        *lengthOut = g_fake.lastComTransmitLength;
    }
    return TRUE;
}

uint32 PduRFake_GetCanIfRxIndicationCount(void) {
    return g_fake.canIfRxIndicationCount;
}

boolean PduRFake_GetLastCanIfRxIndicationId(PduIdType *out) {
    if ((out == NULL) || (!g_fake.hasLastCanIfRxIndicationId)) {
        return FALSE;
    }
    *out = g_fake.lastCanIfRxIndicationId;
    return TRUE;
}

uint32 PduRFake_GetCanIfTxConfirmationCount(void) {
    return g_fake.canIfTxConfirmationCount;
}

boolean PduRFake_GetLastCanIfTxConfirmationId(PduIdType *out) {
    if ((out == NULL) || (!g_fake.hasLastCanIfTxConfirmationId)) {
        return FALSE;
    }
    *out = g_fake.lastCanIfTxConfirmationId;
    return TRUE;
}
