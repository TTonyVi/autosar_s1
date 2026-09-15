/**
 * @file SysTimer.c
 * @brief Ubuntu implementation of the dedicated OS system timer.
 *
 * Deviation from real AUTOSAR, documented deliberately: on real hardware
 * this module would configure a timer peripheral and read its counter
 * register (or service its interrupt). Here it reads `CLOCK_MONOTONIC` and
 * reports ticks elapsed since `SysTimer_Init`.
 *
 * This file and `Gpt.c` are the only two callers of `clock_gettime` in the
 * project; both are under `bsw/mcal/`, so REQ-ARCH-009 holds. They keep
 * separate start timestamps on purpose — see SysTimer.h.
 */

/* Same rationale as Gpt.c: the project builds -std=c11 with extensions
 * disabled, so POSIX.1b clock_gettime must be requested explicitly. */
#define _POSIX_C_SOURCE 199309L

#include "SysTimer.h"

#include <stddef.h>
#include <time.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define SYSTIMER_API_ID_INIT ((Det_ApiIdType)1U)
#define SYSTIMER_API_ID_GET_TICKS ((Det_ApiIdType)2U)

static boolean g_initialized = FALSE;
static struct timespec g_startTime;

static SysTimer_TickType TimespecDiffTicks(const struct timespec *later,
                                           const struct timespec *earlier) {
    int64_t secDiff = (int64_t)later->tv_sec - (int64_t)earlier->tv_sec;
    int64_t nsecDiff = (int64_t)later->tv_nsec - (int64_t)earlier->tv_nsec;
    int64_t ms = (secDiff * 1000) + (nsecDiff / 1000000);
    if (ms < 0) {
        ms = 0; /* CLOCK_MONOTONIC never runs backwards; guards only against
                   a read racing its own init, impossible single-threaded. */
    }
    return (SysTimer_TickType)((uint64_t)ms * (uint64_t)SYSTIMER_TICKS_PER_MS);
}

void SysTimer_Init(const SysTimer_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_SYSTIMER, 0U, SYSTIMER_API_ID_INIT,
                              DET_ERR_NULL_POINTER);
        /* Fall through and still start: see SysTimer.h. */
    } else if ((config->channelId != (uint8)SYSTIMER_CHANNEL_OS) ||
               (config->ticksPerMs != (uint32)SYSTIMER_TICKS_PER_MS)) {
        (void)Det_ReportError(DET_MODULE_ID_SYSTIMER, 0U, SYSTIMER_API_ID_INIT,
                              DET_ERR_INVALID_CONFIG);
    } else {
        /* config consistent, nothing extra to program on this host */
    }

    (void)clock_gettime(CLOCK_MONOTONIC, &g_startTime);
    g_initialized = TRUE;
}

void SysTimer_DeInit(void) {
    g_initialized = FALSE;
}

SysTimer_TickType SysTimer_GetTicks(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_SYSTIMER, 0U, SYSTIMER_API_ID_GET_TICKS,
                              DET_ERR_UNINIT);
        return 0U;
    }

    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return TimespecDiffTicks(&now, &g_startTime);
}
