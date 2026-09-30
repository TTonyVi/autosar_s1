#include "CanIf_Fake.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    uint32 rxIndicationCount;
    boolean hasLastRx;
    Can_HwType lastMailbox;
    uint8 lastData[8];
    uint8 lastDataLength;

    uint32 txConfirmationCount;
    boolean hasLastTxConfirmed;
    PduIdType lastTxConfirmedPduId;

    Std_ReturnType transmitResult;
    uint32 transmitCount;
    boolean hasLastTransmit;
    PduIdType lastTransmitId;
    uint8 lastTransmitData[8];
    uint8 lastTransmitLength;
} CanIfFake_StateType;

static CanIfFake_StateType g_fake;

Std_ReturnType CanIf_Transmit(PduIdType tx_pdu_id, const PduInfoType *info) {
    g_fake.transmitCount++;
    if (info != NULL) {
        g_fake.lastTransmitId = tx_pdu_id;
        uint8 len = (info->SduLength <= 8U) ? (uint8)info->SduLength : 8U;
        (void)memset(g_fake.lastTransmitData, 0, sizeof(g_fake.lastTransmitData));
        if ((info->SduDataPtr != NULL) && (len > 0U)) {
            (void)memcpy(g_fake.lastTransmitData, info->SduDataPtr, len);
        }
        g_fake.lastTransmitLength = len;
        g_fake.hasLastTransmit = TRUE;
    }
    return g_fake.transmitResult;
}

void CanIf_RxIndication(const Can_HwType *mailbox, const PduInfoType *info) {
    g_fake.rxIndicationCount++;
    if ((mailbox == NULL) || (info == NULL)) {
        return;
    }
    g_fake.lastMailbox = *mailbox;
    uint8 len = (info->SduLength <= 8U) ? (uint8)info->SduLength : 8U;
    if ((info->SduDataPtr != NULL) && (len > 0U)) {
        (void)memcpy(g_fake.lastData, info->SduDataPtr, len);
    }
    g_fake.lastDataLength = len;
    g_fake.hasLastRx = TRUE;
}

void CanIf_TxConfirmation(PduIdType tx_pdu_id) {
    g_fake.txConfirmationCount++;
    g_fake.lastTxConfirmedPduId = tx_pdu_id;
    g_fake.hasLastTxConfirmed = TRUE;
}

void CanIfFake_Reset(void) {
    (void)memset(&g_fake, 0, sizeof(g_fake));
    g_fake.transmitResult = E_OK;
}

uint32 CanIfFake_GetRxIndicationCount(void) {
    return g_fake.rxIndicationCount;
}

boolean CanIfFake_GetLastRxMailbox(Can_HwType *out) {
    if ((out == NULL) || (!g_fake.hasLastRx)) {
        return FALSE;
    }
    *out = g_fake.lastMailbox;
    return TRUE;
}

boolean CanIfFake_GetLastRxData(uint8 *buffer, uint8 bufferSize, uint8 *lengthOut) {
    if (!g_fake.hasLastRx) {
        return FALSE;
    }
    uint8 copyLen = (g_fake.lastDataLength <= bufferSize) ? g_fake.lastDataLength : bufferSize;
    if ((buffer != NULL) && (copyLen > 0U)) {
        (void)memcpy(buffer, g_fake.lastData, copyLen);
    }
    if (lengthOut != NULL) {
        *lengthOut = g_fake.lastDataLength;
    }
    return TRUE;
}

uint32 CanIfFake_GetTxConfirmationCount(void) {
    return g_fake.txConfirmationCount;
}

boolean CanIfFake_GetLastTxConfirmedPduId(PduIdType *out) {
    if ((out == NULL) || (!g_fake.hasLastTxConfirmed)) {
        return FALSE;
    }
    *out = g_fake.lastTxConfirmedPduId;
    return TRUE;
}

void CanIfFake_SetTransmitResult(Std_ReturnType result) {
    g_fake.transmitResult = result;
}

uint32 CanIfFake_GetTransmitCount(void) {
    return g_fake.transmitCount;
}

boolean CanIfFake_GetLastTransmitId(PduIdType *out) {
    if ((out == NULL) || (!g_fake.hasLastTransmit)) {
        return FALSE;
    }
    *out = g_fake.lastTransmitId;
    return TRUE;
}

boolean CanIfFake_GetLastTransmitData(uint8 *buffer, uint8 bufferSize, uint8 *lengthOut) {
    if (!g_fake.hasLastTransmit) {
        return FALSE;
    }
    uint8 copyLen =
        (g_fake.lastTransmitLength <= bufferSize) ? g_fake.lastTransmitLength : bufferSize;
    if ((buffer != NULL) && (copyLen > 0U)) {
        (void)memcpy(buffer, g_fake.lastTransmitData, copyLen);
    }
    if (lengthOut != NULL) {
        *lengthOut = g_fake.lastTransmitLength;
    }
    return TRUE;
}
