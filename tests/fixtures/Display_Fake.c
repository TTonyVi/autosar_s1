#include "Display_Fake.h"

#include <stddef.h>
#include <string.h>

static uint8 g_lastFrame[DISPLAY_MAX_FRAME_LENGTH];
static uint16 g_lastLength;
static boolean g_hasFrame;
static uint32 g_writeCallCount;
static uint32 g_successCount;
static uint32 g_failureCount;
static Std_ReturnType g_writeResult = E_OK;

void Display_Init(const Display_ConfigType *config) {
    (void)config;
    DisplayFake_Reset();
}

void Display_DeInit(void) {
    /* nothing to release in the fake */
}

Std_ReturnType Display_Write(const Display_FrameType *frame) {
    if (frame == NULL) {
        return E_NOT_OK;
    }
    g_writeCallCount++;
    if (frame->length <= DISPLAY_MAX_FRAME_LENGTH) {
        (void)memcpy(g_lastFrame, frame->data, frame->length);
        g_lastLength = frame->length;
        g_hasFrame = TRUE;
    }
    if (g_writeResult == E_OK) {
        g_successCount++;
    } else {
        g_failureCount++;
    }
    return g_writeResult;
}

void Display_MainFunction(void) {
    /* nothing periodic in the fake */
}

uint32 Display_GetWriteSuccessCount(void) {
    return g_successCount;
}

uint32 Display_GetWriteFailureCount(void) {
    return g_failureCount;
}

uint32 Display_GetWriteThrottledCount(void) {
    return 0U; /* the fake never throttles; throttling is test_display's job */
}

void DisplayFake_Reset(void) {
    (void)memset(g_lastFrame, 0, sizeof(g_lastFrame));
    g_lastLength = 0U;
    g_hasFrame = FALSE;
    g_writeCallCount = 0U;
    g_successCount = 0U;
    g_failureCount = 0U;
    g_writeResult = E_OK;
}

void DisplayFake_SetWriteResult(Std_ReturnType result) {
    g_writeResult = result;
}

boolean DisplayFake_GetLastFrame(char *buffer, uint16 bufferSize) {
    if ((buffer == NULL) || (bufferSize == 0U) || (!g_hasFrame)) {
        return FALSE;
    }
    uint16 copyLength =
        (g_lastLength < (uint16)(bufferSize - 1U)) ? g_lastLength : (uint16)(bufferSize - 1U);
    (void)memcpy(buffer, g_lastFrame, copyLength);
    buffer[copyLength] = '\0';
    return TRUE;
}

uint32 DisplayFake_GetWriteCallCount(void) {
    return g_writeCallCount;
}
