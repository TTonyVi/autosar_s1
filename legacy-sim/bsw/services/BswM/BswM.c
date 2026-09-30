/**
 * @file BswM.c
 * @brief Basic Software Mode Manager (BSW Mode Manager) implementation.
 *
 * AUTOSAR Classic Architecture:
 * BswM is responsible for Phase 2 (Post-OS) initialization and mode arbitration
 * through Action Lists.
 */
#include "BswM.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "Logger.h"

/* Provided by the ECU-specific RTE under rte/generated/<Ecu>/ */
extern Std_ReturnType Rte_Start(void);
extern Std_ReturnType Rte_Stop(void);

#define LOG_INFO(...)  Logger_Log(LOGGER_LEVEL_INFO, "BswM", __VA_ARGS__)
#define LOG_WARN(...)  Logger_Log(LOGGER_LEVEL_WARN, "BswM", __VA_ARGS__)
#define LOG_ERROR(...) Logger_Log(LOGGER_LEVEL_ERROR, "BswM", __VA_ARGS__)

static const BswM_ConfigType *g_config;
static BswM_StateType g_state = BSWM_STATE_UNINIT;

/* =========================================================================
 * BswM Action Lists (AUTOSAR 4.x Action List pattern)
 * ========================================================================= */

/**
 * Action List 1: Initialize MCAL I/O and CAN drivers.
 */
static Std_ReturnType BswM_ActionList_InitDrivers(const BswM_ConfigType *config) {
    LOG_INFO("executing ActionList: InitDrivers");

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

    if (config->canConfig != NULL) {
        Can_Init(config->canConfig);
        if (!Can_IsControllerReady(config->canControllerId)) {
            LOG_ERROR("CAN controller %u not ready; interface could not be opened",
                      (unsigned)config->canControllerId);
            return E_NOT_OK;
        }
    }

    return E_OK;
}

/**
 * Action List 2: Initialize ECU Abstraction Layer.
 */
static void BswM_ActionList_InitEcuAb(const BswM_ConfigType *config) {
    LOG_INFO("executing ActionList: InitEcuAb");
    if (config->ioHwAbConfig != NULL) {
        IoHwAb_Init(config->ioHwAbConfig);
    }
}

/**
 * Action List 3: Initialize Communication Stack bottom-up (CanIf -> CanSM -> PduR -> Com -> ComM).
 */
static void BswM_ActionList_InitCom(const BswM_ConfigType *config) {
    LOG_INFO("executing ActionList: InitCom");
    if (config->canIfConfig != NULL) {
        CanIf_Init(config->canIfConfig);
    }
    if (config->canSMConfig != NULL) {
        CanSM_Init(config->canSMConfig);
    }
    if (config->pduRConfig != NULL) {
        PduR_Init(config->pduRConfig);
    }
    if (config->comConfig != NULL) {
        Com_Init(config->comConfig);
    }
    if (config->comMConfig != NULL) {
        (void)ComM_Init(config->comMConfig);
    }
}

/**
 * Action List 4: Initialize and start the Runtime Environment (RTE).
 */
static Std_ReturnType BswM_ActionList_InitRte(void) {
    LOG_INFO("executing ActionList: InitRte");
    if (Rte_Start() != E_OK) {
        LOG_ERROR("Rte_Start failed");
        return E_NOT_OK;
    }
    return E_OK;
}

/**
 * Action List 5: Start communication via ComM (AUTOSAR 4.x standard).
 */
static Std_ReturnType BswM_ActionList_StartCommunication(const BswM_ConfigType *config) {
    LOG_INFO("executing ActionList: StartCommunication via ComM");
    if (config->comMConfig != NULL) {
        if (ComM_RequestComMode(COMM_USER_BSWM, COMM_FULL_COMMUNICATION) != E_OK) {
            LOG_ERROR("ComM_RequestComMode FULL_COMMUNICATION failed");
            return E_NOT_OK;
        }
    } else {
        /* Fallback if ComM is not configured */
        if (CanIf_SetControllerMode(config->canControllerId, CAN_CS_STARTED) != E_OK) {
            LOG_ERROR("CAN controller %u refused to start", (unsigned)config->canControllerId);
            return E_NOT_OK;
        }
        Com_IpduGroupStart(COM_DEFAULT_IPDU_GROUP_ID, TRUE);
    }
    return E_OK;
}

/**
 * Action List: Shutdown / De-initialization in reverse order via ComM.
 */
static void BswM_ActionList_Shutdown(const BswM_ConfigType *config) {
    LOG_INFO("executing ActionList: Shutdown");

    if ((config != NULL) && (config->comMConfig != NULL)) {
        (void)ComM_RequestComMode(COMM_USER_BSWM, COMM_NO_COMMUNICATION);
        ComM_DeInit();
        if (config->canSMConfig != NULL) {
            CanSM_DeInit();
        }
    } else {
        Com_IpduGroupStop(COM_DEFAULT_IPDU_GROUP_ID);
        if (config != NULL) {
            (void)CanIf_SetControllerMode(config->canControllerId, CAN_CS_STOPPED);
        }
    }

    (void)Rte_Stop();
    Com_DeInit();
    PduR_DeInit();
    CanIf_DeInit();
    IoHwAb_DeInit();
    Can_DeInit();
    VirtualHw_DeInit();
}

/* =========================================================================
 * BswM Public APIs
 * ========================================================================= */

Std_ReturnType BswM_Init(const BswM_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_BSWM, 0U, BSWM_API_ID_INIT, BSWM_E_NULL_POINTER);
        g_state = BSWM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    g_config = config;
    g_state = BSWM_STATE_STARTUP;
    LOG_INFO("Phase 2 Post-OS startup begins");

    /* 1. Hardware & MCAL Drivers */
    if (BswM_ActionList_InitDrivers(config) != E_OK) {
        (void)Det_ReportError(DET_MODULE_ID_BSWM, 0U, BSWM_API_ID_INIT, BSWM_E_ACTION_FAILED);
        g_state = BSWM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 2. ECU Abstraction */
    BswM_ActionList_InitEcuAb(config);

    /* 3. Communication Stack */
    BswM_ActionList_InitCom(config);

    /* 4. Runtime Environment */
    if (BswM_ActionList_InitRte() != E_OK) {
        (void)Det_ReportError(DET_MODULE_ID_BSWM, 0U, BSWM_API_ID_INIT, BSWM_E_ACTION_FAILED);
        g_state = BSWM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    /* 5. Communication Control */
    if (BswM_ActionList_StartCommunication(config) != E_OK) {
        (void)Det_ReportError(DET_MODULE_ID_BSWM, 0U, BSWM_API_ID_INIT, BSWM_E_ACTION_FAILED);
        g_state = BSWM_STATE_SHUTDOWN;
        return E_NOT_OK;
    }

    g_state = BSWM_STATE_RUN;
    LOG_INFO("Phase 2 complete, mode transitioned to BSWM_STATE_RUN");
    return E_OK;
}

void BswM_DeInit(void) {
    if ((g_state == BSWM_STATE_SHUTDOWN) || (g_state == BSWM_STATE_UNINIT)) {
        return;
    }
    g_state = BSWM_STATE_SHUTDOWN;
    BswM_ActionList_Shutdown(g_config);
    g_config = NULL;
    g_state = BSWM_STATE_UNINIT;
    LOG_INFO("BswM shutdown complete");
}

BswM_StateType BswM_GetState(void) {
    return g_state;
}

void BswM_MainFunction(void) {
    if (g_state != BSWM_STATE_RUN) {
        return;
    }
    ComM_MainFunction();
    CanSM_MainFunction();
}
