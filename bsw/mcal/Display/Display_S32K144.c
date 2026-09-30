/**
 * @file Display_S32K144.c
 * @brief AUTOSAR Display driver implementation for NXP S32K144 — LPUART1
 * text transport.
 *
 * Transmits each frame IoHwAb hands it (the "STATE=...;SPEED=...;FLAGS=
 * ...;T=..." ASCII wire format) verbatim over LPUART1, which on the
 * S32K144-EVB is routed to the OpenSDA USB-serial debug port (PTC6/PTC7,
 * ALT2) — plug the board into USB and the frames are visible in any
 * serial terminal, no extra display hardware required. Verify PTC6/PTC7
 * routing against your own board before relying on it.
 */
#include "Display.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "Gpt.h"
#include "S32K144.h"

#define DISPLAY_API_ID_INIT ((Det_ApiIdType)1U)
#define DISPLAY_API_ID_WRITE ((Det_ApiIdType)2U)
#define DISPLAY_API_ID_MAIN_FUNCTION ((Det_ApiIdType)3U)

static Display_ConfigType g_config;
static boolean g_initialized;
static boolean g_hasWritten;
static Gpt_ValueType g_lastWriteTimeMs;
static uint32 g_writeSuccessCount;
static uint32 g_writeFailureCount;
static uint32 g_writeThrottledCount;

static void LPUART1_Hw_Init(void) {
    /* Clock LPUART1 from SOSCDIV2 (PCS = 1), same 4 MHz source Gpt_S32K144
     * derives its LPIT0 tick rate from. */
    PCC->PCC[PCC_LPUART1_INDEX] = PCC_CGC_MASK | (1UL << PCC_PCS_SHIFT);

    /* LPUART1 on PTC6 (RX, ALT2) / PTC7 (TX, ALT2). */
    PORTC->PCR[6] = PORT_PCR_MUX(2);
    PORTC->PCR[7] = PORT_PCR_MUX(2);

    /* 4 MHz / (16 * 26) ~= 9615 baud (~0.16% off nominal 9600 — within
     * standard UART tolerance). */
    LPUART1->BAUD = LPUART_BAUD_OSR(16U) | LPUART_BAUD_SBR(26U);
    LPUART1->CTRL = LPUART_CTRL_TE_MASK;
}

static void LPUART1_Hw_WriteByte(uint8 byte) {
    while ((LPUART1->STAT & LPUART_STAT_TDRE_MASK) == 0U) {
        /* wait for the transmit data register to empty */
    }
    LPUART1->DATA = (uint32)byte;
}

void Display_Init(const Display_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_DISPLAY, 0U, DISPLAY_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if (config->maxFrameLength > DISPLAY_MAX_FRAME_LENGTH) {
        (void)Det_ReportError(DET_MODULE_ID_DISPLAY, 0U, DISPLAY_API_ID_INIT,
                              DET_ERR_INVALID_CONFIG);
        return;
    }

    LPUART1_Hw_Init();

    g_config = *config;
    g_hasWritten = FALSE;
    g_lastWriteTimeMs = 0U;
    g_writeSuccessCount = 0U;
    g_writeFailureCount = 0U;
    g_writeThrottledCount = 0U;
    g_initialized = TRUE;
}

void Display_DeInit(void) {
    LPUART1->CTRL = 0U;
    g_initialized = FALSE;
}

/* Throttling behaves exactly as the host implementation (section 7.4):
 * a write closer than minWritePeriodMs to the previous one is dropped,
 * not sent, and counted separately from a transport failure. Unlike the
 * host's VirtualHw_Send (which can fail with "no listener"), a blocking
 * UART write has no such failure mode — g_writeFailureCount stays 0
 * unless a future revision adds a real transport error path. */
Std_ReturnType Display_Write(const Display_FrameType *frame) {
    if (frame == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_DISPLAY, 0U, DISPLAY_API_ID_WRITE,
                              DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_DISPLAY, 0U, DISPLAY_API_ID_WRITE, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if (frame->length > g_config.maxFrameLength) {
        (void)Det_ReportError(DET_MODULE_ID_DISPLAY, 0U, DISPLAY_API_ID_WRITE,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    if (g_hasWritten && ((now - g_lastWriteTimeMs) < g_config.minWritePeriodMs)) {
        g_writeThrottledCount++;
        return E_NOT_OK;
    }

    for (uint16 i = 0U; i < frame->length; i++) {
        LPUART1_Hw_WriteByte(frame->data[i]);
    }
    LPUART1_Hw_WriteByte((uint8)'\r');
    LPUART1_Hw_WriteByte((uint8)'\n');

    g_hasWritten = TRUE;
    g_lastWriteTimeMs = now;
    g_writeSuccessCount++;
    return E_OK;
}

void Display_MainFunction(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_DISPLAY, 0U, DISPLAY_API_ID_MAIN_FUNCTION,
                              DET_ERR_UNINIT);
    }
}

uint32 Display_GetWriteSuccessCount(void) {
    return g_writeSuccessCount;
}

uint32 Display_GetWriteFailureCount(void) {
    return g_writeFailureCount;
}

uint32 Display_GetWriteThrottledCount(void) {
    return g_writeThrottledCount;
}
