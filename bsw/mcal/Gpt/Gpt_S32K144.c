/**
 * @file Gpt_S32K144.c
 * @brief AUTOSAR Gpt driver implementation for NXP S32K144.
 *
 * Configures LPIT0 (Low-Power Interrupt Timer) Channel 0 as a hardware
 * millisecond timer base (separate from OS SysTick, complying with REQ-ARCH-014).
 */

#include "Gpt.h"
#include "S32K144.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define GPT_API_ID_INIT             ((Det_ApiIdType)1U)
#define GPT_API_ID_GET_TIME_ELAPSED ((Det_ApiIdType)2U)

/* LPIT0 Channel 0 Clock = SOSCDIV2 = 4 MHz -> 4,000 ticks per millisecond */
#define LPIT_TICKS_PER_MS           (4000UL)

static boolean g_gptInitialized = FALSE;
static uint32 g_lpitInitialVal = 0U;
static uint32 g_elapsedMsAccum = 0U;
static uint32 g_lastCval = 0U;

void Gpt_Init(const Gpt_ConfigType *config) {
    (void)config;

    /* 1. Ensure LPIT0 clock gate is enabled via PCC with SOSCDIV2 (PCS = 1) */
    PCC->PCC[PCC_LPIT_INDEX] = PCC_CGC_MASK | (1UL << PCC_PCS_SHIFT);

    /* 2. Enable LPIT0 Module in MCR */
    LPIT0->MCR = LPIT_MCR_M_CEN_MASK;

    /* 3. Configure Channel 0 as a free-running 32-bit periodic counter starting at max */
    LPIT0->CHANNEL[0].TVAL = 0xFFFFFFFFUL;
    LPIT0->CHANNEL[0].TCTRL = LPIT_TCTRL_T_EN_MASK;

    g_lpitInitialVal = 0xFFFFFFFFUL;
    g_lastCval = 0xFFFFFFFFUL;
    g_elapsedMsAccum = 0U;
    g_gptInitialized = TRUE;
}

void Gpt_DeInit(void) {
    LPIT0->CHANNEL[0].TCTRL = 0U;
    LPIT0->MCR = 0U;
    g_gptInitialized = FALSE;
}

Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType channel) {
    if (channel != GPT_CHANNEL_TIME_BASE) {
        (void)Det_ReportError(DET_MODULE_ID_GPT, 0U, GPT_API_ID_GET_TIME_ELAPSED, DET_ERR_INVALID_PARAM);
        return 0U;
    }
    if (!g_gptInitialized) {
        (void)Det_ReportError(DET_MODULE_ID_GPT, 0U, GPT_API_ID_GET_TIME_ELAPSED, DET_ERR_UNINIT);
        return 0U;
    }

    /* LPIT is a down-counter (counts down from TVAL to 0) */
    uint32 currentCval = LPIT0->CHANNEL[0].CVAL;
    uint32 deltaTicks = 0U;

    if (g_lastCval >= currentCval) {
        deltaTicks = g_lastCval - currentCval;
    } else {
        /* Underflow / Wrap-around */
        deltaTicks = (g_lastCval + (0xFFFFFFFFUL - currentCval)) + 1U;
    }

    g_lastCval = currentCval;
    g_elapsedMsAccum += (deltaTicks / LPIT_TICKS_PER_MS);

    return g_elapsedMsAccum;
}
