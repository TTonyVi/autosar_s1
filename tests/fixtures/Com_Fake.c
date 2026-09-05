#include "Com_Fake.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    uint32 rxIndicationCount;
    boolean hasLastRx;
    PduIdType lastRxId;
    uint8 lastRxData[8];
    uint8 lastRxLength;

    uint32 txConfirmationCount;
    boolean hasLastTxConfirmed;
    PduIdType lastTxConfirmedId;
} ComFake_StateType;

static ComFake_StateType g_fake;

void Com_RxIndication(PduIdType rx_pdu_id, const PduInfoType *pdu_info_ptr) {
    g_fake.rxIndicationCount++;
    g_fake.lastRxId = rx_pdu_id;
    if (pdu_info_ptr != NULL) {
        uint8 len = (pdu_info_ptr->SduLength <= 8U) ? (uint8)pdu_info_ptr->SduLength : 8U;
        (void)memset(g_fake.lastRxData, 0, sizeof(g_fake.lastRxData));
        if ((pdu_info_ptr->SduDataPtr != NULL) && (len > 0U)) {
            (void)memcpy(g_fake.lastRxData, pdu_info_ptr->SduDataPtr, len);
        }
        g_fake.lastRxLength = len;
    }
    g_fake.hasLastRx = TRUE;
}

void Com_TxConfirmation(PduIdType tx_pdu_id) {
    g_fake.txConfirmationCount++;
    g_fake.lastTxConfirmedId = tx_pdu_id;
    g_fake.hasLastTxConfirmed = TRUE;
}

void ComFake_Reset(void) {
    (void)memset(&g_fake, 0, sizeof(g_fake));
}

uint32 ComFake_GetRxIndicationCount(void) {
    return g_fake.rxIndicationCount;
}

boolean ComFake_GetLastRxIndicationId(PduIdType *out) {
    if ((out == NULL) || (!g_fake.hasLastRx)) {
        return FALSE;
    }
    *out = g_fake.lastRxId;
    return TRUE;
}

boolean ComFake_GetLastRxIndicationData(uint8 *buffer, uint8 bufferSize, uint8 *lengthOut) {
    if (!g_fake.hasLastRx) {
        return FALSE;
    }
    uint8 copyLen = (g_fake.lastRxLength <= bufferSize) ? g_fake.lastRxLength : bufferSize;
    if ((buffer != NULL) && (copyLen > 0U)) {
        (void)memcpy(buffer, g_fake.lastRxData, copyLen);
    }
    if (lengthOut != NULL) {
        *lengthOut = g_fake.lastRxLength;
    }
    return TRUE;
}

uint32 ComFake_GetTxConfirmationCount(void) {
    return g_fake.txConfirmationCount;
}

boolean ComFake_GetLastTxConfirmationId(PduIdType *out) {
    if ((out == NULL) || (!g_fake.hasLastTxConfirmed)) {
        return FALSE;
    }
    *out = g_fake.lastTxConfirmedId;
    return TRUE;
}
