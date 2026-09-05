#include "Display.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "Gpt.h"
#include "VirtualHw.h"

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

    g_config = *config;
    g_hasWritten = FALSE;
    g_lastWriteTimeMs = 0U;
    g_writeSuccessCount = 0U;
    g_writeFailureCount = 0U;
    g_writeThrottledCount = 0U;
    g_initialized = TRUE;
}

void Display_DeInit(void) {
    g_initialized = FALSE;
}

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

    Std_ReturnType result =
        VirtualHw_Send(g_config.virtualHwLinkId, (const char *)frame->data, frame->length);
    g_hasWritten = TRUE;
    g_lastWriteTimeMs = now;
    if (result == E_OK) {
        g_writeSuccessCount++;
    } else {
        /* Non-fatal by design (REQ-VHW-005, UC-11): a missing HMI listener
         * is a normal, expected condition, not an ECU error. */
        g_writeFailureCount++;
    }
    return result;
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
