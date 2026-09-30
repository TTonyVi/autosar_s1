#include "Os.h"

#include <stddef.h>
#include <string.h>

#include "Det.h"
#include "Det_ModuleIds.h"

/* The scheduler reads time ONLY through Os_Counter (spec section 8.1);
 * `Gpt` belongs to the other BSW modules and must not appear here. */
/* Vai trò: đồng hồ mili-giây riêng của scheduler — CHỈ đọc qua
 * Os_Counter, không bao giờ gọi Gpt (REQ-ARCH-014). */
static Os_MilliType OsNowMs(void) {
    return Os_Counter_TicksToMs(Os_Counter_GetValue());
}

#if !defined(__arm__) && !defined(__thumb__)
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <time.h>
#endif

#include "BswM.h"
#include "EcuM.h"

#define OS_API_ID_INIT             ((Det_ApiIdType)1U)
#define OS_API_ID_MAIN_FUNCTION    ((Det_ApiIdType)2U)
#define OS_API_ID_GET_TASK_STATS   ((Det_ApiIdType)3U)
#define OS_API_ID_START_OS         ((Det_ApiIdType)4U)
#define OS_API_ID_SHUTDOWN_OS      ((Det_ApiIdType)5U)

typedef struct {
    uint32 nextReleaseMs;
    uint32 invocationCount;
    uint32 maxStartLatencyMs;
    uint32 maxExecTimeMs;
    uint32 missedCount;
} Os_TaskRuntimeType;

static const Os_ConfigType *g_config;
static Os_TaskRuntimeType g_runtime[OS_MAX_TASKS];
static boolean g_initialized;

/* Vai trò: nạp bảng task tĩnh (callback + chu kỳ + priority), validate,
 * rồi tính mốc "release lần đầu" cho từng task = now + period (không
 * release ngay lúc init). */
Std_ReturnType Os_Init(const Os_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, OS_API_ID_INIT, DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }
    if ((config->tasks == NULL) || (config->numTasks == 0U) || (config->numTasks > OS_MAX_TASKS)) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, OS_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return E_NOT_OK;
    }
    for (uint8 i = 0U; i < config->numTasks; i++) {
        if ((config->tasks[i].callback == NULL) || (config->tasks[i].periodMs == 0U)) {
            (void)Det_ReportError(DET_MODULE_ID_OS, i, OS_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            return E_NOT_OK;
        }
    }

    g_config = config;
    (void)memset(g_runtime, 0, sizeof(g_runtime));

    Os_MilliType now = OsNowMs();
    for (uint8 i = 0U; i < config->numTasks; i++) {
        /* First release happens one period after Os_Init, not immediately
         * at init time — a deterministic, documented convention. */
        g_runtime[i].nextReleaseMs = now + config->tasks[i].periodMs;
    }

    g_initialized = TRUE;
    return E_OK;
}

/* Vai trò: chuẩn AUTOSAR/OSEK StartOS — khởi động OS, chạy StartupHook,
 * và bước vào vòng lặp điều phối của OS (hàm không quay lại). */
void StartOS(AppModeType Mode) {
    (void)Mode;
    if (!g_initialized || (g_config == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, OS_API_ID_START_OS, DET_ERR_UNINIT);
        return;
    }

    /* 1. Thực thi StartupHook (gọi EcuM_StartupTwo & BswM_Init để khởi động Phase 2) */
    if (g_config->startupHook != NULL) {
        g_config->startupHook();
    }

    /* 2. Kiểm tra Phase 2 hoàn tất thành công qua BswM */
    if (BswM_GetState() != BSWM_STATE_RUN) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, OS_API_ID_START_OS, DET_ERR_INVALID_CONFIG);
        ShutdownOS(E_OS_STATE);
        return;
    }

    /* 3. Vòng lặp điều phối chính của OS — chiếm quyền CPU và chạy liên tục */
    while (!EcuM_ShutdownRequested()) {
        EcuM_MainFunction();

        if (EcuM_IsDurationExpired()) {
            EcuM_RequestShutdown();
            break;
        }

#if defined(__arm__) || defined(__thumb__)
        /* Trên vi điều khiển ARM (S32K144): ngủ WFI tiết kiệm điện cho đến ngắt SysTick 1ms kế tiếp */
        __asm volatile("wfi");
#else
        /* Trên máy chủ mô phỏng (Ubuntu/Linux): ngủ 250µs định kỳ để điều hòa xung nhịp */
        const struct timespec tick = {.tv_sec = 0, .tv_nsec = 250000L};
        (void)nanosleep(&tick, NULL);
#endif
    }

    /* 4. Tắt máy tuần tự khi có yêu cầu dừng */
    ShutdownOS(E_OK);
}

/* Vai trò: chuẩn AUTOSAR/OSEK ShutdownOS — chạy ShutdownHook, tắt EcuM và thoát/halt. */
void ShutdownOS(StatusType Error) {
    if ((g_config != NULL) && (g_config->shutdownHook != NULL)) {
        g_config->shutdownHook(Error);
    }

    EcuM_RequestShutdown();
    EcuM_Shutdown();

#if defined(__arm__) || defined(__thumb__)
    while (1) {
        __asm volatile("wfi");
    }
#elif !defined(UNIT_TEST)
    exit((int)Error);
#endif
}

/* Vai trò: tắt scheduler (hạ cờ init, bỏ tham chiếu config). */
void Os_DeInit(void) {
    g_initialized = FALSE;
    g_config = NULL;
}

/* Vai trò: TRÁI TIM của scheduler — gọi mỗi tick của vòng lặp chính.
 * Bước 1: với từng task, so mốc "next release" với "now" bằng timestamp
 * TUYỆT ĐỐI (không phải now+period) để không bị trôi dồn (cumulative
 * drift); nếu bỏ lỡ nhiều hơn 1 lần release thì chỉ chạy 1 lần, phần còn
 * lại tính là "missed" + báo lỗi (REQ-OS-002). Bước 2: chạy các task đang
 * "due" theo đúng thứ tự ưu tiên (priority cao chạy trước), đo lại
 * latency khởi động + thời gian thực thi để phục vụ thống kê. */
void Os_MainFunction(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, OS_API_ID_MAIN_FUNCTION, DET_ERR_UNINIT);
        return;
    }

    Os_MilliType now = OsNowMs();

    boolean due[OS_MAX_TASKS] = {FALSE};
    uint32 originalDue[OS_MAX_TASKS] = {0U};
    uint8 dueCount = 0U;

    /* Pass 1: advance each task's release schedule past `now`, using
     * absolute next-release timestamps (never "now + period") so a late
     * check never accumulates drift (REQ: "Use absolute next-release
     * timestamps to avoid cumulative drift"). If more than one release was
     * due, run once and record the rest as missed (REQ-OS-002). */
    for (uint8 i = 0U; i < g_config->numTasks; i++) {
        Os_TaskRuntimeType *rt = &g_runtime[i];
        uint32 firstDue = rt->nextReleaseMs;
        uint32 missedThisCycle = 0U;

        while (rt->nextReleaseMs <= now) {
            missedThisCycle++;
            rt->nextReleaseMs += g_config->tasks[i].periodMs;
        }

        if (missedThisCycle > 0U) {
            due[i] = TRUE;
            originalDue[i] = firstDue;
            dueCount++;
            if (missedThisCycle > 1U) {
                rt->missedCount += (missedThisCycle - 1U);
                (void)Det_ReportError(DET_MODULE_ID_OS, i, OS_API_ID_MAIN_FUNCTION,
                                      DET_ERR_SCHEDULER_OVERRUN);
            }
        }
    }

    /* Pass 2: execute due tasks in priority order (larger value first),
     * ties broken by array position — earlier wins (see Os.h). */
    for (uint8 round = 0U; round < dueCount; round++) {
        sint16 bestIdx = -1;
        for (uint8 i = 0U; i < g_config->numTasks; i++) {
            if (due[i]) {
                if ((bestIdx < 0) ||
                    (g_config->tasks[i].priority > g_config->tasks[(uint8)bestIdx].priority)) {
                    bestIdx = (sint16)i;
                }
            }
        }

        uint8 idx = (uint8)bestIdx;
        Os_TaskRuntimeType *rt = &g_runtime[idx];

        uint32 latency = now - originalDue[idx];
        if (latency > rt->maxStartLatencyMs) {
            rt->maxStartLatencyMs = latency;
        }

        Os_MilliType execStart = OsNowMs();
        g_config->tasks[idx].callback();
        Os_MilliType execEnd = OsNowMs();
        uint32 execTime = execEnd - execStart;
        if (execTime > rt->maxExecTimeMs) {
            rt->maxExecTimeMs = execTime;
        }

        rt->invocationCount++;
        due[idx] = FALSE;
    }
}

/* Vai trò: getter — trả về thống kê 1 task (số lần chạy, latency/exec
 * time tối đa từng ghi nhận, số lần bị miss) phục vụ quan sát/debug. */
Std_ReturnType Os_GetTaskStats(uint8 taskIndex, Os_TaskStatsType *out) {
    if ((out == NULL) || (!g_initialized) || (g_config == NULL) ||
        (taskIndex >= g_config->numTasks)) {
        (void)Det_ReportError(DET_MODULE_ID_OS, taskIndex, OS_API_ID_GET_TASK_STATS,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    const Os_TaskRuntimeType *rt = &g_runtime[taskIndex];
    out->invocationCount = rt->invocationCount;
    out->maxStartLatencyMs = rt->maxStartLatencyMs;
    out->maxExecTimeMs = rt->maxExecTimeMs;
    out->missedCount = rt->missedCount;
    out->nextReleaseMs = rt->nextReleaseMs;
    return E_OK;
}
