#include "EcuM.h"

#include <stddef.h>

#include "Dem.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "Os_Counter.h"

#define ECUM_API_ID_INIT ((Det_ApiIdType)1U)
#define ECUM_API_ID_MAIN_FUNCTION ((Det_ApiIdType)2U)

/* Provided by the ECU-specific RTE under rte/generated/<Ecu>/. The symbol
 * names are identical on both ECUs by design (section 5.3), which is what
 * lets this module stay generic and ECU-agnostic (REQ-ARCH-005). */
extern Std_ReturnType Rte_Start(void);
extern Std_ReturnType Rte_Stop(void);

static const EcuM_ConfigType *g_config;
static EcuM_StateType g_state = ECUM_STATE_OFF;
static volatile boolean g_shutdownRequested;

#define LOG_INFO(...) Logger_Log(LOGGER_LEVEL_INFO, "EcuM", __VA_ARGS__)
#define LOG_WARN(...) Logger_Log(LOGGER_LEVEL_WARN, "EcuM", __VA_ARGS__)
#define LOG_ERROR(...) Logger_Log(LOGGER_LEVEL_ERROR, "EcuM", __VA_ARGS__)

/* Vai trò: NHẠC TRƯỞNG Phase 1 (Pre-OS) — khởi tạo các module nền tảng
 * trước khi có OS: Logger → Mcu/clock → Port → SysTimer/Os_Counter → Gpt
 * → Det/Dem → Os_Init → StartOS (gọi StartupHook để BswM chạy Phase 2). */
Std_ReturnType EcuM_Init(const EcuM_ConfigType *config) {
    g_shutdownRequested = FALSE;
    g_config = NULL;
    g_state = ECUM_STATE_STARTUP;

    if ((config == NULL) || (config->osConfig == NULL)) {
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 1. Logger first, so every later failure is reportable rather than silently lost */
    Logger_Init(config->ecuName, config->logLevel);
    LOG_INFO("Phase 1 Pre-OS startup begins");

    /* 2. Microcontroller and clock: nothing else can run without them. */
    Mcu_Init(config->mcuConfig);
    if (Mcu_InitClock(MCU_CLOCK_DEFAULT) != E_OK) {
        LOG_ERROR("Mcu_InitClock failed");
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 3. Pin configuration before any peripheral use. */
    Port_Init(config->portConfig);

    /* 4-5. The OS time base, before the general-purpose one. */
    SysTimer_Init(config->sysTimerConfig);
    Os_Counter_Init();

    /* 6. General-purpose time base for the other BSW modules. */
    Gpt_Init(config->gptConfig);
    Logger_NotifyTimeBaseReady();

    /* 7-8. Diagnostics, so later failures are recordable. */
    Det_Init();
    Dem_Init();
    LOG_INFO("time base and diagnostics up");
    LOG_INFO("log level=%u", (unsigned)config->logLevel);

    /* 9. OS schedule setup. */
    if (Os_Init(config->osConfig) != E_OK) {
        LOG_ERROR("Os_Init rejected the task table");
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    g_config = config;

    /* 10. StartOS: hands control permanently to OS (does not return on normal execution) */
    LOG_INFO("Phase 1 Pre-OS complete, invoking StartOS");
    StartOS(OSDEFAULTAPPMODE);

    /* Reached only if StartOS was forced to return early */
    return (g_state == ECUM_STATE_RUN) ? E_OK : E_NOT_OK;
}

/* Vai trò: Phase 2 transition — được gọi từ OS context / StartupHook khi OS đã chạy. */
void EcuM_StartupTwo(void) {
    g_state = ECUM_STATE_RUN;
    LOG_INFO("EcuM_StartupTwo: transitioning to RUN");
}

/* Vai trò: hàm chạy theo chu kỳ của EcuM — gọi BswM_MainFunction và Os_MainFunction. */
void EcuM_MainFunction(void) {
    if (g_state != ECUM_STATE_RUN) {
        (void)Det_ReportError(DET_MODULE_ID_ECUM, 0U, ECUM_API_ID_MAIN_FUNCTION, DET_ERR_UNINIT);
        return;
    }
    BswM_MainFunction();
    Os_MainFunction();
}

/* Vai trò: chỉ đặt 1 cờ boolean (REQ-ECUM-003) — an toàn để gọi từ signal
 * handler (Ctrl+C), không I/O, không cấp phát bộ nhớ. */
void EcuM_RequestShutdown(void) {
    g_shutdownRequested = TRUE;
}

/* Vai trò: getter — cờ shutdown đã được yêu cầu chưa. */
boolean EcuM_ShutdownRequested(void) {
    return g_shutdownRequested;
}

/* Vai trò: kiểm tra giới hạn thời gian chạy (--duration) nếu có cấu hình. */
boolean EcuM_IsDurationExpired(void) {
    if ((g_config == NULL) || (g_config->durationSeconds == 0U)) {
        return FALSE;
    }
    Gpt_ValueType durationMs = g_config->durationSeconds * 1000U;
    if (Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE) >= durationMs) {
        LOG_INFO("--duration reached, stopping");
        return TRUE;
    }
    return FALSE;
}

/* Vai trò: getter — trạng thái hiện tại của ECU. */
EcuM_StateType EcuM_GetState(void) {
    return g_state;
}

/* Vai trò: tắt máy — phối hợp với BswM_DeInit tắt các module Post-OS,
 * sau đó tắt các module Phase 1 Pre-OS theo thứ tự ngược lại. */
void EcuM_Shutdown(void) {
    if ((g_state == ECUM_STATE_SHUTDOWN) || (g_state == ECUM_STATE_OFF)) {
        return;
    }

    g_state = ECUM_STATE_POST_RUN;
    LOG_INFO("shutdown begins");

    /* 1. De-initialize Post-OS modules via BswM */
    BswM_DeInit();

    /* 2. Scheduler and diagnostic summary while those modules are still up */
    if (g_config != NULL) {
        for (uint8 i = 0U; i < g_config->osConfig->numTasks; i++) {
            Os_TaskStatsType stats;
            if (Os_GetTaskStats(i, &stats) == E_OK) {
                LOG_INFO("task %s: runs=%u missed=%u max_latency=%ums max_exec=%ums",
                         g_config->osConfig->tasks[i].name, (unsigned)stats.invocationCount,
                         (unsigned)stats.missedCount, (unsigned)stats.maxStartLatencyMs,
                         (unsigned)stats.maxExecTimeMs);
            }
        }
    }
    LOG_INFO("det errors=%u, log lines=%u", (unsigned)Det_GetTotalErrorCount(),
             (unsigned)Logger_GetEmittedCount());
    uint32 records = Det_GetRecordCount();
    for (uint32 i = 0U; i < records; i++) {
        Det_RecordType record;
        if (Det_GetRecord(i, &record) == E_OK) {
            LOG_WARN("det record %u: module=%u instance=%u api=%u error=%u", (unsigned)i,
                     (unsigned)record.moduleId, (unsigned)record.instanceId, (unsigned)record.apiId,
                     (unsigned)record.errorId);
        }
    }
    Dem_DeInit();
    Det_DeInit();

    g_state = ECUM_STATE_SHUTDOWN;
    LOG_INFO("shutdown complete");

    /* 3. Tear down Phase 1 time bases and OS */
    Os_DeInit();
    Gpt_DeInit();
    Os_Counter_DeInit();
    SysTimer_DeInit();

    Logger_DeInit();
    g_config = NULL;
}
