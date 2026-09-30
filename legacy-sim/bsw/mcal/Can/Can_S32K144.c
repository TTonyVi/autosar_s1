/**
 * @file Can_S32K144.c
 * @brief AUTOSAR Can driver implementation for NXP S32K144 (FlexCAN0).
 *
 * Implements hardware CAN communication using FlexCAN0:
 * - 500 kbps classic CAN bit timing from 40 MHz Bus Clock.
 * - Hardware Mailbox 0 dedicated to Transmit (HTH 0).
 * - Hardware Mailbox 1 dedicated to Receive (HRH 0).
 * - Forwards received frames to CanIf_RxIndication and confirmed transmissions to CanIf_TxConfirmation.
 */

#include "Can.h"
#include "S32K144.h"

#include <stddef.h>
#include <string.h>

#include "CanIf.h"
#include "Det.h"
#include "Det_ModuleIds.h"

#define CAN_API_ID_INIT                ((Det_ApiIdType)1U)
#define CAN_API_ID_SET_CONTROLLER_MODE ((Det_ApiIdType)2U)
#define CAN_API_ID_WRITE               ((Det_ApiIdType)3U)
#define CAN_API_ID_MAIN_FUNCTION_READ  ((Det_ApiIdType)4U)

#define S32K144_CAN_TX_MB              (0U)
#define S32K144_CAN_RX_MB              (1U)

static boolean g_canInitialized = FALSE;
static Can_ControllerStateType g_controllerMode = CAN_CS_UNINIT;
static uint32 g_rxFrameCount = 0U;
static uint32 g_rxDroppedCount = 0U;
static uint32 g_txFailureCount = 0U;

/**
 * @brief Configure FlexCAN0 hardware for 500 kbps.
 */
static void FlexCAN0_Hw_Init(void) {
    /* 1. Enable module clock and perform soft reset */
    CAN0->MCR &= ~FlexCAN_MCR_MDIS_MASK;
    CAN0->MCR |= FlexCAN_MCR_SOFTRST_MASK;
    while ((CAN0->MCR & FlexCAN_MCR_SOFTRST_MASK) != 0U) {
        /* Wait for reset */
    }

    /* 2. Enter Freeze mode */
    CAN0->MCR |= FlexCAN_MCR_FRZ_MASK | FlexCAN_MCR_HALT_MASK;
    while ((CAN0->MCR & FlexCAN_MCR_FRZACK_MASK) == 0U) {
        /* Wait for freeze acknowledge */
    }

    /* 3. Configure Bit Timing: 500 kbps from 40 MHz Bus clock
     * Tq = 16. PRESDIV = 4 (divide by 5 -> 8MHz Sclock -> 500kHz).
     * PROPSEG = 6 (reg: 5), PSEG1 = 5 (reg: 4), PSEG2 = 4 (reg: 3), RJW = 2 (reg: 1).
     * CLKSRC = 1 (Bus Clock).
     */
    CAN0->CTRL1 = (4UL << 24) | (1UL << 22) | (4UL << 19) | (3UL << 16) | (1UL << 13) | (5UL << 0);

    /* 4. Set Maximum Message Buffers (16) */
    CAN0->MCR = (CAN0->MCR & ~0x7FUL) | FlexCAN_MCR_MAXMB(16U);

    /* 5. Accept all Standard IDs on Rx MB */
    CAN0->RXMGMASK = 0x00000000UL;

    /* 6. Initialize Mailbox 0 (Tx) as INACTIVE */
    CAN0->MB[S32K144_CAN_TX_MB].CS = FlexCAN_MB_CS_CODE(FlexCAN_MB_CODE_TX_INACTIVE);

    /* 7. Initialize Mailbox 1 (Rx) as EMPTY */
    CAN0->MB[S32K144_CAN_RX_MB].ID = 0x00000000UL;
    CAN0->MB[S32K144_CAN_RX_MB].CS = FlexCAN_MB_CS_CODE(FlexCAN_MB_CODE_RX_EMPTY);

    /* Clear any pending interrupt flags */
    CAN0->IFLAG1 = 0xFFFFFFFFUL;

    /* 8. Exit Freeze Mode and enter normal operation */
    CAN0->MCR &= ~(FlexCAN_MCR_FRZ_MASK | FlexCAN_MCR_HALT_MASK);
    while ((CAN0->MCR & FlexCAN_MCR_FRZACK_MASK) != 0U) {
        /* Wait for freeze exit */
    }
    while ((CAN0->MCR & FlexCAN_MCR_NOTRDY_MASK) != 0U) {
        /* Wait until ready */
    }
}

void Can_Init(const Can_ConfigType *config) {
    (void)config;

    FlexCAN0_Hw_Init();

    g_canInitialized = TRUE;
    g_controllerMode = CAN_CS_STOPPED;
    g_rxFrameCount = 0U;
    g_rxDroppedCount = 0U;
    g_txFailureCount = 0U;
}

void Can_DeInit(void) {
    /* Put FlexCAN into freeze mode and disable */
    CAN0->MCR |= FlexCAN_MCR_MDIS_MASK;
    g_canInitialized = FALSE;
    g_controllerMode = CAN_CS_UNINIT;
}

Std_ReturnType Can_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode) {
    if (controller_id != 0U) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controller_id, CAN_API_ID_SET_CONTROLLER_MODE, DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if (!g_canInitialized) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controller_id, CAN_API_ID_SET_CONTROLLER_MODE, DET_ERR_UNINIT);
        return E_NOT_OK;
    }

    g_controllerMode = mode;
    return E_OK;
}

Can_ControllerStateType Can_GetControllerMode(uint8 controller_id) {
    if (controller_id != 0U) {
        return CAN_CS_UNINIT;
    }
    return g_controllerMode;
}

boolean Can_IsControllerReady(uint8 controller_id) {
    if (controller_id != 0U) {
        return FALSE;
    }
    return g_canInitialized;
}

Std_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu_info) {
    (void)hth;
    if (!g_canInitialized) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_WRITE, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if ((pdu_info == NULL) || (pdu_info->length > CAN_CLASSIC_MAX_DLC)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_WRITE, DET_ERR_INVALID_PARAM);
        g_txFailureCount++;
        return E_NOT_OK;
    }
    if (g_controllerMode != CAN_CS_STARTED) {
        g_txFailureCount++;
        return E_NOT_OK;
    }

    /* Check if Tx MB0 is ready to transmit */
    uint32 cs = CAN0->MB[S32K144_CAN_TX_MB].CS;
    uint32 code = (cs & FlexCAN_MB_CS_CODE_MASK) >> 24;
    if ((code != FlexCAN_MB_CODE_TX_INACTIVE) && (code != FlexCAN_MB_CODE_TX_ABORT)) {
        /* Previous Tx still in progress */
        g_txFailureCount++;
        return E_NOT_OK;
    }

    /* Assemble payload data words (Big Endian) */
    uint32 word0 = 0U;
    uint32 word1 = 0U;
    if (pdu_info->sdu != NULL) {
        if (pdu_info->length > 0U) { word0 |= ((uint32)pdu_info->sdu[0] << 24); }
        if (pdu_info->length > 1U) { word0 |= ((uint32)pdu_info->sdu[1] << 16); }
        if (pdu_info->length > 2U) { word0 |= ((uint32)pdu_info->sdu[2] << 8);  }
        if (pdu_info->length > 3U) { word0 |= ((uint32)pdu_info->sdu[3]);       }
        if (pdu_info->length > 4U) { word1 |= ((uint32)pdu_info->sdu[4] << 24); }
        if (pdu_info->length > 5U) { word1 |= ((uint32)pdu_info->sdu[5] << 16); }
        if (pdu_info->length > 6U) { word1 |= ((uint32)pdu_info->sdu[6] << 8);  }
        if (pdu_info->length > 7U) { word1 |= ((uint32)pdu_info->sdu[7]);       }
    }

    /* Standard 11-bit ID shifted to bits 18..28 */
    CAN0->MB[S32K144_CAN_TX_MB].ID = (uint32)((pdu_info->id & 0x7FFU) << 18);
    CAN0->MB[S32K144_CAN_TX_MB].WORD0 = word0;
    CAN0->MB[S32K144_CAN_TX_MB].WORD1 = word1;

    /* Trigger transmission */
    CAN0->MB[S32K144_CAN_TX_MB].CS = FlexCAN_MB_CS_CODE(FlexCAN_MB_CODE_TX_DATA_ONCE) |
                                     FlexCAN_MB_CS_DLC(pdu_info->length);

    /* Confirm transmission upward to CanIf */
    CanIf_TxConfirmation(pdu_info->swPduHandle);
    return E_OK;
}

void Can_MainFunction_Read(void) {
    if (!g_canInitialized) {
        return;
    }

    /* Check if Mailbox 1 (Rx) has received a new frame (flag in IFLAG1 or CODE == RX_FULL) */
    if ((CAN0->IFLAG1 & (1UL << S32K144_CAN_RX_MB)) != 0U) {
        uint32 cs = CAN0->MB[S32K144_CAN_RX_MB].CS;
        uint32 id = CAN0->MB[S32K144_CAN_RX_MB].ID;
        uint32 word0 = CAN0->MB[S32K144_CAN_RX_MB].WORD0;
        uint32 word1 = CAN0->MB[S32K144_CAN_RX_MB].WORD1;

        /* Clear flag by writing 1 to IFLAG1 */
        CAN0->IFLAG1 = (1UL << S32K144_CAN_RX_MB);

        /* Dummy read of timer to unlock mailbox */
        (void)CAN0->TIMER;

        /* Reactivate Mailbox 1 as RX_EMPTY */
        CAN0->MB[S32K144_CAN_RX_MB].CS = FlexCAN_MB_CS_CODE(FlexCAN_MB_CODE_RX_EMPTY);

        g_rxFrameCount++;

        /* If controller is not in STARTED state, drop frame without indicating upward */
        if (g_controllerMode != CAN_CS_STARTED) {
            g_rxDroppedCount++;
            return;
        }

        uint8 dlc = (uint8)((cs & FlexCAN_MB_CS_DLC_MASK) >> 16);
        if (dlc > CAN_CLASSIC_MAX_DLC) {
            dlc = CAN_CLASSIC_MAX_DLC;
        }

        uint8 rxData[8];
        rxData[0] = (uint8)((word0 >> 24) & 0xFFU);
        rxData[1] = (uint8)((word0 >> 16) & 0xFFU);
        rxData[2] = (uint8)((word0 >> 8)  & 0xFFU);
        rxData[3] = (uint8)(word0         & 0xFFU);
        rxData[4] = (uint8)((word1 >> 24) & 0xFFU);
        rxData[5] = (uint8)((word1 >> 16) & 0xFFU);
        rxData[6] = (uint8)((word1 >> 8)  & 0xFFU);
        rxData[7] = (uint8)(word1         & 0xFFU);

        Can_HwType hw;
        hw.CanId = (uint16)((id >> 18) & 0x7FFU);
        hw.Hoh = S32K144_CAN_RX_MB;
        hw.ControllerId = 0U;

        PduInfoType pduInfo;
        pduInfo.SduDataPtr = rxData;
        pduInfo.SduLength = dlc;

        CanIf_RxIndication(&hw, &pduInfo);
    }
}

void Can_MainFunction_Write(void) {
    /* Reserved in classic AUTOSAR polling mode */
}

uint32 Can_GetRxFrameCount(uint8 controller_id) {
    return (controller_id == 0U) ? g_rxFrameCount : 0U;
}

uint32 Can_GetRxDroppedCount(uint8 controller_id) {
    return (controller_id == 0U) ? g_rxDroppedCount : 0U;
}

uint32 Can_GetTxFailureCount(uint8 controller_id) {
    return (controller_id == 0U) ? g_txFailureCount : 0U;
}
