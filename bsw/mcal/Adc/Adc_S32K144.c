/**
 * @file Adc_S32K144.c
 * @brief AUTOSAR Adc driver implementation for NXP S32K144.
 *
 * Configures ADC0 for 12-bit single-ended conversion on Channel 7 (PTB12 / ADC0_SE7,
 * e.g., potentiometer on S32K144-EVB).
 */

#include "Adc.h"
#include "S32K144.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define ADC_API_ID_INIT               ((Det_ApiIdType)1U)
#define ADC_API_ID_READ_GROUP         ((Det_ApiIdType)2U)
#define ADC_API_ID_GET_GROUP_STATUS   ((Det_ApiIdType)3U)
#define ADC_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)4U)
#define ADC_API_ID_GET_AGE            ((Det_ApiIdType)5U)

#define ADC0_CHANNEL_SE7              (7U)

static boolean g_initialized = FALSE;
static Adc_StatusType g_status = ADC_IDLE;
static uint16 g_latestValue = 0U;
static Gpt_ValueType g_lastSampleMs = 0U;
static uint32 g_readCount = 0U;

void Adc_Init(const Adc_ConfigType *config) {
    (void)config;

    /* Configure ADC0:
     * - CFG1: 12-bit mode (MODE=1), clock divide by 1 (ADIV=0), input clock = Bus clock (ADICLK=0)
     * - CFG2: Default sample time
     * - SC2: Software trigger (ADTRG=0)
     * - SC3: Single conversion (ADCO=0)
     */
    ADC0->CFG1 = 0x00000004UL; /* MODE=1 (12-bit) */
    ADC0->CFG2 = 0x0000000CUL; /* Sample time duration */
    ADC0->SC2  = 0x00000000UL; /* Software trigger, default voltage reference */
    ADC0->SC3  = 0x00000000UL; /* Single conversion mode */

    /* Trigger first conversion on Channel 7 (PTB12) */
    ADC0->SC1[0] = ADC_SC1_ADCH(ADC0_CHANNEL_SE7);

    g_status = ADC_IDLE;
    g_latestValue = 0U;
    g_lastSampleMs = 0U;
    g_readCount = 0U;
    g_initialized = TRUE;
}

void Adc_DeInit(void) {
    /* Disable ADC conversions by selecting channel 31 (disabled) */
    ADC0->SC1[0] = ADC_SC1_ADCH(31U);
    g_initialized = FALSE;
    g_status = ADC_IDLE;
}

Std_ReturnType Adc_ReadGroup(Adc_GroupType group, Adc_ValueGroupType *data_buffer) {
    (void)group;
    if (!g_initialized || (data_buffer == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, 0U, ADC_API_ID_READ_GROUP, DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    *data_buffer = g_latestValue;
    return E_OK;
}

Adc_StatusType Adc_GetGroupStatus(Adc_GroupType group) {
    (void)group;
    if (!g_initialized) {
        return ADC_IDLE;
    }
    return g_status;
}

Gpt_ValueType Adc_GetGroupAge(Adc_GroupType group) {
    (void)group;
    if (!g_initialized || (g_status == ADC_IDLE)) {
        return 0xFFFFFFFFUL;
    }
    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    return (now >= g_lastSampleMs) ? (now - g_lastSampleMs) : 0U;
}

void Adc_MainFunction_Read(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, 0U, ADC_API_ID_MAIN_FUNCTION_READ, DET_ERR_UNINIT);
        return;
    }

    /* Check if conversion on SC1[0] is complete (COCO flag) */
    if ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) != 0U) {
        /* Read 12-bit result (0..4095) - reading R[0] clears COCO */
        g_latestValue = (uint16)(ADC0->R[0] & 0x0FFFUL);
        g_status = ADC_STREAM_CONVERTED;
        g_lastSampleMs = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
        g_readCount++;

        /* Trigger next conversion on channel 7 */
        ADC0->SC1[0] = ADC_SC1_ADCH(ADC0_CHANNEL_SE7);
    }
}

uint32 Adc_GetGroupReadCount(Adc_GroupType group) {
    (void)group;
    return g_readCount;
}
