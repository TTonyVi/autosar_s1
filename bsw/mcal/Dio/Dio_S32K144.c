/**
 * @file Dio_S32K144.c
 * @brief AUTOSAR Dio driver implementation for NXP S32K144.
 *
 * Directly reads S32K144 GPIO hardware input registers:
 * - Channel 0: Ignition Switch / SW2 (PTC12)
 * - Channel 1: Red LED (PTD15)
 * - Channel 2: Green LED (PTD16)
 * - Channel 3: Blue LED (PTD0)
 */

#include "Dio.h"
#include "S32K144.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define DIO_API_ID_INIT               ((Det_ApiIdType)1U)
#define DIO_API_ID_READ_CHANNEL       ((Det_ApiIdType)2U)
#define DIO_API_ID_GET_AGE            ((Det_ApiIdType)3U)
#define DIO_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)4U)

#define DIO_MAX_CHANNELS ((uint8)4U)

static boolean g_initialized = FALSE;
static Gpt_ValueType g_lastSampleMs = 0U;
static boolean g_sampleReceived = FALSE;

void Dio_Init(const Dio_ConfigType *config) {
    (void)config;
    g_initialized = TRUE;
    g_sampleReceived = TRUE;
    g_lastSampleMs = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
}

void Dio_DeInit(void) {
    g_initialized = FALSE;
    g_sampleReceived = FALSE;
}

Dio_LevelType Dio_ReadChannel(Dio_ChannelType channel_id) {
    if (!g_initialized || (channel_id >= DIO_MAX_CHANNELS)) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, channel_id, DIO_API_ID_READ_CHANNEL, DET_ERR_INVALID_PARAM);
        return STD_LOW;
    }

    switch (channel_id) {
        case 0U:
            /* PTC12 (SW2 Push Button, active-low pull-up):
             * If bit is 0, button pressed -> return STD_HIGH (Ignition ON)
             * If bit is 1, button released -> return STD_LOW (Ignition OFF) */
            return ((PTC->PDIR & (1UL << 12)) == 0U) ? STD_HIGH : STD_LOW;

        case 1U:
            /* PTD15 (Red LED) */
            return ((PTD->PDIR & (1UL << 15)) != 0U) ? STD_HIGH : STD_LOW;

        case 2U:
            /* PTD16 (Green LED) */
            return ((PTD->PDIR & (1UL << 16)) != 0U) ? STD_HIGH : STD_LOW;

        case 3U:
            /* PTD0 (Blue LED) */
            return ((PTD->PDIR & (1UL << 0)) != 0U) ? STD_HIGH : STD_LOW;

        default:
            return STD_LOW;
    }
}

Gpt_ValueType Dio_GetChannelAge(Dio_ChannelType channel_id) {
    if (!g_initialized || (channel_id >= DIO_MAX_CHANNELS)) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, channel_id, DIO_API_ID_GET_AGE, DET_ERR_INVALID_PARAM);
        return 0xFFFFFFFFUL;
    }
    if (!g_sampleReceived) {
        return 0xFFFFFFFFUL;
    }
    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    return (now >= g_lastSampleMs) ? (now - g_lastSampleMs) : 0U;
}

void Dio_MainFunction_Read(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, 0U, DIO_API_ID_MAIN_FUNCTION_READ, DET_ERR_UNINIT);
        return;
    }
    g_lastSampleMs = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    g_sampleReceived = TRUE;
}
