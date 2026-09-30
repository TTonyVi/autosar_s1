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

/* ------------------------------------------------------------------
 * Second fake role: the signal API and reception counters the RTE reads.
 * Used by test_rte_sensor and test_rte_cluster; unused by test_pdur.
 * ------------------------------------------------------------------ */

#define COM_FAKE_MAX_SIGNALS 8U
#define COM_FAKE_MAX_IPDUS 4U

static uint16 g_sentSignal[COM_FAKE_MAX_SIGNALS];
static boolean g_sentSignalValid[COM_FAKE_MAX_SIGNALS];
static uint16 g_receivedSignal[COM_FAKE_MAX_SIGNALS];
static uint32 g_sendSignalCount;
static uint8 g_sendResult = E_OK;
static uint8 g_receiveResult = E_OK;

static boolean g_rxTimedOut[COM_FAKE_MAX_IPDUS];
static uint32 g_rxValidCount[COM_FAKE_MAX_IPDUS];
static uint32 g_rxCrcErrorCount[COM_FAKE_MAX_IPDUS];
static uint32 g_rxCounterErrorCount[COM_FAKE_MAX_IPDUS];
static uint32 g_rxSignalRangeErrorCount[COM_FAKE_MAX_IPDUS];

uint8 Com_SendSignal(Com_SignalIdType signal_id, const void *signal_data_ptr) {
    if ((signal_data_ptr == NULL) || (signal_id >= (Com_SignalIdType)COM_FAKE_MAX_SIGNALS)) {
        return E_NOT_OK;
    }
    g_sentSignal[signal_id] = *(const uint16 *)signal_data_ptr;
    g_sentSignalValid[signal_id] = TRUE;
    g_sendSignalCount++;
    return g_sendResult;
}

uint8 Com_ReceiveSignal(Com_SignalIdType signal_id, void *signal_data_ptr) {
    if ((signal_data_ptr == NULL) || (signal_id >= (Com_SignalIdType)COM_FAKE_MAX_SIGNALS)) {
        return E_NOT_OK;
    }
    *(uint16 *)signal_data_ptr = g_receivedSignal[signal_id];
    return g_receiveResult;
}

boolean Com_GetIpduRxTimeoutStatus(Com_IPduIdType ipdu_id, boolean *isTimedOut) {
    /* Matches the real Com.h contract: boolean TRUE on success. */
    if ((isTimedOut == NULL) || (ipdu_id >= (Com_IPduIdType)COM_FAKE_MAX_IPDUS)) {
        return FALSE;
    }
    *isTimedOut = g_rxTimedOut[ipdu_id];
    return TRUE;
}

uint32 Com_GetIpduRxValidCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) ? g_rxValidCount[ipdu_id] : 0U;
}

uint32 Com_GetIpduRxCrcErrorCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) ? g_rxCrcErrorCount[ipdu_id] : 0U;
}

uint32 Com_GetIpduRxCounterErrorCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) ? g_rxCounterErrorCount[ipdu_id] : 0U;
}

uint32 Com_GetIpduRxSignalRangeErrorCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) ? g_rxSignalRangeErrorCount[ipdu_id] : 0U;
}

uint32 ComFake_GetSendSignalCount(void) {
    return g_sendSignalCount;
}

boolean ComFake_GetLastSentSignal(Com_SignalIdType signal_id, uint16 *out) {
    if ((out == NULL) || (signal_id >= (Com_SignalIdType)COM_FAKE_MAX_SIGNALS) ||
        (!g_sentSignalValid[signal_id])) {
        return FALSE;
    }
    *out = g_sentSignal[signal_id];
    return TRUE;
}

void ComFake_SetSendSignalResult(uint8 result) {
    g_sendResult = result;
}

void ComFake_SetReceivedSignal(Com_SignalIdType signal_id, uint16 value) {
    if (signal_id < (Com_SignalIdType)COM_FAKE_MAX_SIGNALS) {
        g_receivedSignal[signal_id] = value;
    }
}

void ComFake_SetReceiveSignalResult(uint8 result) {
    g_receiveResult = result;
}

void ComFake_SetRxTimeout(Com_IPduIdType ipdu_id, boolean timedOut) {
    if (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) {
        g_rxTimedOut[ipdu_id] = timedOut;
    }
}

void ComFake_SetRxValidCount(Com_IPduIdType ipdu_id, uint32 count) {
    if (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) {
        g_rxValidCount[ipdu_id] = count;
    }
}

void ComFake_SetRxCrcErrorCount(Com_IPduIdType ipdu_id, uint32 count) {
    if (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) {
        g_rxCrcErrorCount[ipdu_id] = count;
    }
}

void ComFake_SetRxCounterErrorCount(Com_IPduIdType ipdu_id, uint32 count) {
    if (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) {
        g_rxCounterErrorCount[ipdu_id] = count;
    }
}

void ComFake_SetRxSignalRangeErrorCount(Com_IPduIdType ipdu_id, uint32 count) {
    if (ipdu_id < (Com_IPduIdType)COM_FAKE_MAX_IPDUS) {
        g_rxSignalRangeErrorCount[ipdu_id] = count;
    }
}

void ComFake_ResetSignals(void) {
    for (uint8 i = 0U; i < (uint8)COM_FAKE_MAX_SIGNALS; i++) {
        g_sentSignal[i] = 0U;
        g_sentSignalValid[i] = FALSE;
        g_receivedSignal[i] = 0U;
    }
    for (uint8 i = 0U; i < (uint8)COM_FAKE_MAX_IPDUS; i++) {
        g_rxTimedOut[i] = FALSE;
        g_rxValidCount[i] = 0U;
        g_rxCrcErrorCount[i] = 0U;
        g_rxCounterErrorCount[i] = 0U;
        g_rxSignalRangeErrorCount[i] = 0U;
    }
    g_sendSignalCount = 0U;
    g_sendResult = E_OK;
    g_receiveResult = E_OK;
}
