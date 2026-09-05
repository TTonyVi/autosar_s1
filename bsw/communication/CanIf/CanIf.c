/**
 * @file CanIf.c
 * @brief CAN Interface implementation: static routing tables, ID/DLC
 * validation, and a controller-state transmit gate. No socket code lives
 * here (section 3.1: CanIf "may call" only the Can Driver).
 */
#include "CanIf.h"

#include <stddef.h>
#include <string.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "PduR.h"

#define CANIF_API_ID_INIT ((Det_ApiIdType)1U)
#define CANIF_API_ID_SET_CONTROLLER_MODE ((Det_ApiIdType)2U)
#define CANIF_API_ID_TRANSMIT ((Det_ApiIdType)3U)
#define CANIF_API_ID_RX_INDICATION ((Det_ApiIdType)4U)
#define CANIF_API_ID_TX_CONFIRMATION ((Det_ApiIdType)5U)

/* Section 6.2: this project has exactly two I-PDUs total (one Tx, one Rx
 * per ECU); sized a little above that for headroom, matching the
 * small-fixed-table convention used by VirtualHw/Adc/Dio/Display. */
#define CANIF_MAX_TX_PDUS ((uint8)4U)
#define CANIF_MAX_RX_PDUS ((uint8)4U)

typedef struct {
    boolean initialized;
    CanIf_ConfigType config; /* shallow copy: the arrays it points to are link-time `const`
                              * tables (section 11.1) owned by the caller and SHALL outlive
                              * CanIf_Init, same convention as every other *_ConfigType in
                              * this project. */

    uint32 rxAcceptedCount;
    uint32 rxRejectedCount;
    uint32 txConfirmationCount;

    boolean hasLastRxPduId;
    PduIdType lastRxPduId;
} CanIf_StateType;

static CanIf_StateType g_state;

void CanIf_Init(const CanIf_ConfigType *config) {
    /* Set unconditionally before any validation, so a re-Init that fails
     * partway through can never leave a PREVIOUS successful init's
     * routing tables silently active. */
    g_state.initialized = FALSE;

    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if (((config->numTxPdus > 0U) && (config->txPdus == NULL)) ||
        ((config->numRxPdus > 0U) && (config->rxPdus == NULL)) ||
        (config->numTxPdus > CANIF_MAX_TX_PDUS) || (config->numRxPdus > CANIF_MAX_RX_PDUS)) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    /* REQ-CFG-001/003/004 and section 7.5: reviewable, validated, const
     * tables; any single bad entry fails the whole init (all-or-nothing,
     * same as Os_Init/Adc_Init/Dio_Init/Display_Init) rather than starting
     * up with a partially-wrong routing table. */
    for (uint8 i = 0U; i < config->numTxPdus; i++) {
        if ((config->txPdus[i].canIfTxPduId != (PduIdType)i) || (config->txPdus[i].dlc == 0U) ||
            (config->txPdus[i].dlc > CAN_CLASSIC_MAX_DLC)) {
            (void)Det_ReportError(DET_MODULE_ID_CANIF, i, CANIF_API_ID_INIT,
                                  DET_ERR_INVALID_CONFIG);
            return;
        }
    }
    for (uint8 i = 0U; i < config->numRxPdus; i++) {
        if ((config->rxPdus[i].canIfRxPduId != (PduIdType)i) || (config->rxPdus[i].dlc == 0U) ||
            (config->rxPdus[i].dlc > CAN_CLASSIC_MAX_DLC)) {
            (void)Det_ReportError(DET_MODULE_ID_CANIF, i, CANIF_API_ID_INIT,
                                  DET_ERR_INVALID_CONFIG);
            return;
        }
        for (uint8 j = 0U; j < i; j++) {
            if ((config->rxPdus[j].canId == config->rxPdus[i].canId) &&
                (config->rxPdus[j].controllerId == config->rxPdus[i].controllerId)) {
                /* REQ-CFG-003: duplicate physical CAN ID in one Rx table. */
                (void)Det_ReportError(DET_MODULE_ID_CANIF, i, CANIF_API_ID_INIT,
                                      DET_ERR_INVALID_CONFIG);
                return;
            }
        }
    }

    g_state.config = *config;
    g_state.rxAcceptedCount = 0U;
    g_state.rxRejectedCount = 0U;
    g_state.txConfirmationCount = 0U;
    g_state.hasLastRxPduId = FALSE;
    g_state.lastRxPduId = 0U;
    g_state.initialized = TRUE;
}

void CanIf_DeInit(void) {
    (void)memset(&g_state, 0, sizeof(g_state));
}

Std_ReturnType CanIf_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode) {
    if (!g_state.initialized) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, controller_id, CANIF_API_ID_SET_CONTROLLER_MODE,
                              DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    return Can_SetControllerMode(controller_id, mode);
}

Std_ReturnType CanIf_Transmit(PduIdType tx_pdu_id, const PduInfoType *info) {
    if (!g_state.initialized) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_TRANSMIT, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if (tx_pdu_id >= g_state.config.numTxPdus) {
        /* Unmapped/out-of-range logical Tx PDU ID. */
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_TRANSMIT,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    const CanIf_TxPduConfigType *txPdu = &g_state.config.txPdus[tx_pdu_id];
    if ((info == NULL) || (info->SduDataPtr == NULL) || (info->SduLength != txPdu->dlc)) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, (uint8)tx_pdu_id, CANIF_API_ID_TRANSMIT,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if (Can_GetControllerMode(txPdu->controllerId) != CAN_CS_STARTED) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, (uint8)tx_pdu_id, CANIF_API_ID_TRANSMIT,
                              DET_ERR_INVALID_CONFIG);
        return E_NOT_OK;
    }

    Can_PduType pdu;
    pdu.swPduHandle = tx_pdu_id;
    pdu.length = (uint8)info->SduLength; /* == txPdu->dlc (uint8), already checked equal above */
    pdu.id = txPdu->canId;
    pdu.sdu = info->SduDataPtr;
    return Can_Write(txPdu->hth, &pdu);
}

void CanIf_RxIndication(const Can_HwType *mailbox, const PduInfoType *info) {
    if (!g_state.initialized) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_RX_INDICATION, DET_ERR_UNINIT);
        return;
    }
    if ((mailbox == NULL) || (info == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_RX_INDICATION,
                              DET_ERR_NULL_POINTER);
        return;
    }

    for (uint8 i = 0U; i < g_state.config.numRxPdus; i++) {
        const CanIf_RxPduConfigType *rxPdu = &g_state.config.rxPdus[i];
        if ((rxPdu->canId == mailbox->CanId) && (rxPdu->controllerId == mailbox->ControllerId)) {
            if (info->SduLength != rxPdu->dlc) {
                g_state.rxRejectedCount++;
                (void)Det_ReportError(DET_MODULE_ID_CANIF, i, CANIF_API_ID_RX_INDICATION,
                                      DET_ERR_INVALID_PARAM);
                return; /* whole PDU rejected on wrong DLC, before any signal is touched */
            }
            g_state.rxAcceptedCount++;
            g_state.lastRxPduId = rxPdu->canIfRxPduId;
            g_state.hasLastRxPduId = TRUE;
            PduR_CanIfRxIndication(rxPdu->canIfRxPduId, info);
            return;
        }
    }

    /* No configured Rx PDU maps this CAN ID/controller pair -- dropped,
     * not fatal (an unmapped ID on a shared bus is expected, not a
     * programming error by itself, but still worth a DET record for
     * visibility per section 10.1's "invalid ... controller ID"). */
    g_state.rxRejectedCount++;
    (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_RX_INDICATION,
                          DET_ERR_INVALID_PARAM);
}

void CanIf_TxConfirmation(PduIdType tx_pdu_id) {
    if (!g_state.initialized) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_TX_CONFIRMATION,
                              DET_ERR_UNINIT);
        return;
    }
    if (tx_pdu_id >= g_state.config.numTxPdus) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, CANIF_API_ID_TX_CONFIRMATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    g_state.txConfirmationCount++;
    PduR_CanIfTxConfirmation(tx_pdu_id);
}

uint32 CanIf_GetRxAcceptedCount(void) {
    return g_state.rxAcceptedCount;
}

uint32 CanIf_GetRxRejectedCount(void) {
    return g_state.rxRejectedCount;
}

uint32 CanIf_GetTxConfirmationCount(void) {
    return g_state.txConfirmationCount;
}

Std_ReturnType CanIf_GetLastRxPduId(PduIdType *out) {
    if ((out == NULL) || (!g_state.hasLastRxPduId)) {
        return E_NOT_OK;
    }
    *out = g_state.lastRxPduId;
    return E_OK;
}
