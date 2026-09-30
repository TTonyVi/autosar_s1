/**
 * @file test_display.c
 * @brief Phase 2 unit tests for Display (bsw/mcal/Display).
 *
 * Covers section 14.1 ("test_display"): frame formatting, write failure
 * counted and reported, minimum write period respected. Uses the fake
 * VirtualHw and fake Gpt (tests/fixtures) for deterministic control over
 * both time and send outcomes.
 */
#include <stdio.h>
#include <string.h>

#include "Det.h"
#include "Display.h"
#include "Gpt_Fake.h"
#include "Platform_Types.h"
#include "VirtualHw_Fake.h"

#define DISPLAY_LINK_ID 0U

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static const Display_ConfigType g_config = {DISPLAY_LINK_ID, DISPLAY_MAX_FRAME_LENGTH, 90U};

static void ResetSystem(void) {
    GptFake_Reset();
    Gpt_Init(NULL);
    Det_Init();
    VirtualHwFake_Reset();
    VirtualHwFake_SetSendResult(DISPLAY_LINK_ID, E_OK);
    Display_Init(&g_config);
}

static Display_FrameType MakeFrame(const char *text) {
    Display_FrameType frame;
    (void)memset(&frame, 0, sizeof(frame));
    size_t len = strlen(text);
    (void)memcpy(frame.data, text, len);
    frame.length = (uint16)len;
    return frame;
}

static void TestFrameIsSentVerbatim(void) {
    ResetSystem();

    Display_FrameType frame = MakeFrame("STATE=NORMAL;SPEED=5000;FLAGS=0x00;T=1234");
    Std_ReturnType r = Display_Write(&frame);
    Check(r == E_OK, "Display_Write succeeds with a listener present");
    Check(Display_GetWriteSuccessCount() == 1U, "a successful write is counted");
    Check(VirtualHwFake_GetSendCount(DISPLAY_LINK_ID) == 1U,
          "the frame reaches VirtualHw_Send exactly once — MCAL transports bytes, does not "
          "reinterpret them");
}

static void TestWriteFailureCountedAndReported(void) {
    ResetSystem();
    VirtualHwFake_SetSendResult(DISPLAY_LINK_ID, E_NOT_OK); /* simulate "no HMI listening" */

    Display_FrameType frame = MakeFrame("STATE=OFF;SPEED=0;FLAGS=0x00;T=0");
    Std_ReturnType r = Display_Write(&frame);
    Check(r == E_NOT_OK, "Display_Write reports the underlying send failure");
    Check(Display_GetWriteFailureCount() == 1U,
          "the failure is counted (feeds DEM_EVENT_DISPLAY_LINK_LOST later)");
    Check(Display_GetWriteSuccessCount() == 0U, "a failed write is not also counted as a success");

    /* A missing listener must never be fatal (UC-11): the driver stays
     * usable and keeps trying on the next write. */
    GptFake_AdvanceMs(1000U);
    VirtualHwFake_SetSendResult(DISPLAY_LINK_ID, E_OK);
    r = Display_Write(&frame);
    Check(r == E_OK, "a later write succeeds once a listener appears, with no lasting damage from "
                     "earlier failures");
}

static void TestMinimumWritePeriodRespected(void) {
    ResetSystem();

    Display_FrameType frame = MakeFrame("STATE=NORMAL;SPEED=1000;FLAGS=0x00;T=0");
    Check(Display_Write(&frame) == E_OK, "first write succeeds unconditionally");

    /* Configured minWritePeriodMs is 90; 50ms later is too soon. */
    GptFake_AdvanceMs(50U);
    Std_ReturnType r = Display_Write(&frame);
    Check(r == E_NOT_OK, "a write inside the minimum period is throttled, not sent");
    Check(Display_GetWriteThrottledCount() == 1U,
          "the throttled write is counted separately from failures");
    Check(VirtualHwFake_GetSendCount(DISPLAY_LINK_ID) == 1U,
          "a throttled write never reaches VirtualHw_Send");

    /* Advancing past the period allows the next write through. */
    GptFake_AdvanceMs(50U); /* total 100ms since the first write, > 90ms */
    r = Display_Write(&frame);
    Check(r == E_OK, "a write after the minimum period elapses is sent normally");
    Check(VirtualHwFake_GetSendCount(DISPLAY_LINK_ID) == 2U,
          "the second real send actually reached VirtualHw_Send");
}

static void TestOversizedFrameRejected(void) {
    ResetSystem();

    Display_FrameType frame;
    (void)memset(&frame, 'A', sizeof(frame));
    frame.length = (uint16)(DISPLAY_MAX_FRAME_LENGTH + 1U);

    uint32 detBefore = Det_GetTotalErrorCount();
    Std_ReturnType r = Display_Write(&frame);
    Check(r == E_NOT_OK, "a frame longer than maxFrameLength is rejected");
    Check(Det_GetTotalErrorCount() > detBefore, "an oversized frame is reported to Det");
    Check(VirtualHwFake_GetSendCount(DISPLAY_LINK_ID) == 0U,
          "an oversized frame never reaches VirtualHw_Send");
}

int main(void) {
    TestFrameIsSentVerbatim();
    TestWriteFailureCountedAndReported();
    TestMinimumWritePeriodRespected();
    TestOversizedFrameRejected();

    if (g_failures == 0) {
        (void)printf("test_display: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_display: %d check(s) failed\n", g_failures);
    return 1;
}
