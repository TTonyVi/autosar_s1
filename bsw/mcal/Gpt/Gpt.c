/**
 * @file Gpt.c
 * @brief Ubuntu implementation of Gpt: the sole caller of `clock_gettime`
 * in this project (see Gpt.h and REQ-ARCH-009's static check in
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 14.3).
 *
 * Deviation from real AUTOSAR: on real hardware this module would read a
 * free-running hardware timer/counter register directly; here it reads
 * `CLOCK_MONOTONIC` and reports elapsed time relative to `Gpt_Init`.
 */

/* Requesting POSIX.1b (clock_gettime, CLOCK_MONOTONIC) explicitly: the
 * project builds with -std=c11 and extensions disabled (CMAKE_C_EXTENSIONS
 * OFF), so glibc's <time.h> would otherwise hide these behind its default,
 * stricter feature-test profile. This define is the only one of its kind
 * in the project, confined to the one file allowed to touch the host
 * clock at all. */
#define _POSIX_C_SOURCE 199309L

#include "Gpt.h"

#include <stddef.h>
#include <time.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define GPT_API_ID_INIT ((Det_ApiIdType)1U)
#define GPT_API_ID_GET_TIME_ELAPSED ((Det_ApiIdType)2U)

static boolean g_initialized = FALSE;
static struct timespec g_startTime;

/* Vai trò: tính hiệu 2 mốc thời gian (struct timespec) ra mili-giây. */
static Gpt_ValueType TimespecDiffMs(const struct timespec *later, const struct timespec *earlier) {
    int64_t secDiff = (int64_t)later->tv_sec - (int64_t)earlier->tv_sec;
    int64_t nsecDiff = (int64_t)later->tv_nsec - (int64_t)earlier->tv_nsec;
    int64_t ms = (secDiff * 1000) + (nsecDiff / 1000000);
    if (ms < 0) {
        ms = 0; /* clock never runs backwards on CLOCK_MONOTONIC; this only
                    guards against a caller reading before Gpt_Init raced
                    with itself, which single-threaded ECUs cannot do. */
    }
    return (Gpt_ValueType)ms;
}

/* Vai trò: khởi động time base — lưu lại mốc "thời điểm 0" bằng
 * clock_gettime(CLOCK_MONOTONIC); mọi lần đọc sau đó là khoảng cách tính
 * từ mốc này. */
void Gpt_Init(const Gpt_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_GPT, 0U, GPT_API_ID_INIT, DET_ERR_NULL_POINTER);
        /* Fall through: the single free-running channel needs no per-
         * channel config to start, so a null config is a reportable
         * mistake but not a reason to refuse starting the time base. */
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &g_startTime);
    g_initialized = TRUE;
}

/* Vai trò: tắt module (chỉ hạ cờ init). */
void Gpt_DeInit(void) {
    g_initialized = FALSE;
}

/* Vai trò: trả về số mili-giây đã trôi qua kể từ lúc Gpt_Init — đây là
 * "đồng hồ mili-giây" mà mọi module BSW khác dùng để tính chu kỳ/timeout
 * (ngoại trừ Os, vốn dùng Os_Counter/SysTimer riêng, không qua Gpt). */
Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType channel) {
    if (channel != GPT_CHANNEL_TIME_BASE) {
        (void)Det_ReportError(DET_MODULE_ID_GPT, 0U, GPT_API_ID_GET_TIME_ELAPSED,
                              DET_ERR_INVALID_PARAM);
        return 0U;
    }
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_GPT, 0U, GPT_API_ID_GET_TIME_ELAPSED, DET_ERR_UNINIT);
        return 0U;
    }

    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return TimespecDiffMs(&now, &g_startTime);
}
