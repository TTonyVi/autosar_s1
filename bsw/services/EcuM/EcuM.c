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

Std_ReturnType EcuM_Init(const EcuM_ConfigType *config) {
    g_shutdownRequested = FALSE;
    g_config = NULL;
    g_state = ECUM_STATE_STARTUP;

    if ((config == NULL) || (config->osConfig == NULL)) {
        /* Nothing is initialized yet, so this cannot be logged or recorded
         * anywhere useful; the return value is the whole report. */
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 1. Logger first, so every later failure is reportable rather than
     *    silently lost (section 9.2). */
    Logger_Init(config->ecuName, config->logLevel);
    LOG_INFO("startup begins");

    /* 2. Microcontroller and clock: nothing else can run without them. */
    Mcu_Init(config->mcuConfig);
    if (Mcu_InitClock(MCU_CLOCK_DEFAULT) != E_OK) {
        LOG_ERROR("Mcu_InitClock failed");
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 3. Pin configuration before any peripheral use. */
    Port_Init(config->portConfig);

    /* 4-5. The OS time base, before the general-purpose one: the scheduler
     *      must be able to keep time before any module Gpt serves exists
     *      (section 8.1, REQ-ARCH-014). */
    SysTimer_Init(config->sysTimerConfig);
    Os_Counter_Init();

    /* 6. General-purpose time base for the other BSW modules. */
    Gpt_Init(config->gptConfig);
    Logger_NotifyTimeBaseReady();

    /* 7-8. Diagnostics, so every later initialization failure is recordable. */
    Det_Init();
    Dem_Init();
    LOG_INFO("time base and diagnostics up");

    /* 9. Post-build-like configuration was parsed by main() and is already
     *    baked into the tables below (section 11.1); logging it here is
     *    what makes the effective configuration visible at startup. */
    LOG_INFO("log level=%u", (unsigned)config->logLevel);

    /* 10. Input and output MCAL drivers. VirtualHw comes first inside this
     *     step because Adc/Dio/Display are its consumers; the spec groups
     *     them as one step rather than naming the transport separately.
     *     REQ-ECUM-005: a missing link is NOT a startup failure. */
    if (config->virtualHwConfig != NULL) {
        VirtualHw_Init(config->virtualHwConfig);
    }
    if (config->adcConfig != NULL) {
        Adc_Init(config->adcConfig);
    }
    if (config->dioConfig != NULL) {
        Dio_Init(config->dioConfig);
    }
    if (config->displayConfig != NULL) {
        Display_Init(config->displayConfig);
    } else {
        LOG_INFO("display driver disabled by configuration");
    }

    /* 11. CAN driver. REQ-ECUM-001: unlike a hardware link, a CAN interface
     *     that cannot be opened IS a fatal startup failure. */
    Can_Init(config->canConfig);
    if (!Can_IsControllerReady(config->canControllerId)) {
        LOG_ERROR("CAN controller %u not ready; the interface could not be opened",
                  (unsigned)config->canControllerId);
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 12. ECU Abstraction, which depends on the drivers above. */
    IoHwAb_Init(config->ioHwAbConfig);

    /* 13-15. Communication stack bottom-up, each layer after the one it
     *        depends on. */
    CanIf_Init(config->canIfConfig);
    PduR_Init(config->pduRConfig);
    Com_Init(config->comConfig);

    /* 16. The RTE is the first layer allowed to reach application code. */
    if (Rte_Start() != E_OK) {
        LOG_ERROR("Rte_Start failed");
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 17. Controller to started, through CanIf (never Can directly). */
    if (CanIf_SetControllerMode(config->canControllerId, CAN_CS_STARTED) != E_OK) {
        LOG_ERROR("CAN controller %u refused to start", (unsigned)config->canControllerId);
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 18. COM I-PDU group with initialization enabled. */
    Com_IpduGroupStart(COM_DEFAULT_IPDU_GROUP_ID, TRUE);

    /* 19. OS schedule last: nothing may be activated before every module
     *     it will call is ready. */
    if (Os_Init(config->osConfig) != E_OK) {
        LOG_ERROR("Os_Init rejected the task table");
        g_state = ECUM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    g_config = config;
    g_state = ECUM_STATE_RUN;
    LOG_INFO("startup complete, entering RUN");
    return E_OK;
}

void EcuM_MainFunction(void) {
    if (g_state != ECUM_STATE_RUN) {
        (void)Det_ReportError(DET_MODULE_ID_ECUM, 0U, ECUM_API_ID_MAIN_FUNCTION, DET_ERR_UNINIT);
        return;
    }
    Os_MainFunction();
}

void EcuM_RequestShutdown(void) {
    /* REQ-ECUM-003: only sets a flag. No I/O, no allocation, nothing that
     * would be unsafe to do from a signal handler. */
    g_shutdownRequested = TRUE;
}

boolean EcuM_ShutdownRequested(void) {
    return g_shutdownRequested;
}

EcuM_StateType EcuM_GetState(void) {
    return g_state;
}

void EcuM_Shutdown(void) {
    if ((g_state == ECUM_STATE_SHUTDOWN) || (g_state == ECUM_STATE_OFF)) {
        /* REQ-ECUM-004: repeated deinit SHALL not crash. */
        return;
    }

    /* Section 9.3, the reverse of the initialization order. */
    g_state = ECUM_STATE_POST_RUN;
    LOG_INFO("shutdown begins");

    /* 1. Stop accepting new application activations. Leaving RUN is what
     *    does that: EcuM_MainFunction refuses to schedule in any other
     *    state. Os_DeInit is deliberately deferred to step 9, because the
     *    scheduler summary at step 8 has to be readable. */

    /* 2-3. Stop the COM group, then the controller. */
    Com_IpduGroupStop(COM_DEFAULT_IPDU_GROUP_ID);
    if (g_config != NULL) {
        (void)CanIf_SetControllerMode(g_config->canControllerId, CAN_CS_STOPPED);
    }

    /* 4-6. RTE, then the communication stack top-down. */
    (void)Rte_Stop();
    Com_DeInit();
    PduR_DeInit();
    CanIf_DeInit();
    Can_DeInit();

    /* 7. Abstraction, then the I/O drivers, then the links themselves. */
    IoHwAb_DeInit();
    if ((g_config != NULL) && (g_config->adcConfig != NULL)) {
        Adc_DeInit();
    }
    if ((g_config != NULL) && (g_config->dioConfig != NULL)) {
        Dio_DeInit();
    }
    if ((g_config != NULL) && (g_config->displayConfig != NULL)) {
        Display_DeInit();
    }
    VirtualHw_DeInit();

    /* 8. Scheduler and diagnostic summary, while those modules are still
     *    up (section 9.3 step 8). */
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

    /* 10. Logged before the time bases stop, so the last line still
     *     carries a real timestamp. */
    g_state = ECUM_STATE_SHUTDOWN;
    LOG_INFO("shutdown complete");

    /* 9. Scheduler, then the time bases, in the reverse of steps 4 to 6. */
    Os_DeInit();
    Gpt_DeInit();
    Os_Counter_DeInit();
    SysTimer_DeInit();

    Logger_DeInit();
    g_config = NULL;
}
