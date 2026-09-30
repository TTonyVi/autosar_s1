/**
 * @file CanSM.c
 * @brief CAN State Manager (CanSM) implementation.
 */
#include "CanSM.h"

#include <stddef.h>
#include <string.h>

#include "CanIf.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "Logger.h"

#define LOG_INFO(...)  Logger_Log(LOGGER_LEVEL_INFO, "CanSM", __VA_ARGS__)
#define LOG_WARN(...)  Logger_Log(LOGGER_LEVEL_WARN, "CanSM", __VA_ARGS__)
#define LOG_ERROR(...) Logger_Log(LOGGER_LEVEL_ERROR, "CanSM", __VA_ARGS__)

static const CanSM_ConfigType *g_config;
static CanSM_StateType g_networkState[CANSM_MAX_NETWORKS];
static boolean g_initialized;

static const CanSM_NetworkConfigType *FindNetwork(NetworkHandleType network) {
    if (g_config == NULL) {
        return NULL;
    }
    for (uint8 i = 0U; i < g_config->numNetworks; i++) {
        if (g_config->networks[i].networkHandle == network) {
            return &g_config->networks[i];
        }
    }
    return NULL;
}

void CanSM_Init(const CanSM_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_CANSM, 0U, CANSM_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }

    g_config = config;
    for (uint8 i = 0U; i < CANSM_MAX_NETWORKS; i++) {
        g_networkState[i] = CANSM_UNINIT;
    }

    for (uint8 i = 0U; i < config->numNetworks; i++) {
        NetworkHandleType net = config->networks[i].networkHandle;
        if (net < CANSM_MAX_NETWORKS) {
            g_networkState[net] = CANSM_NOCOM;
        }
    }

    g_initialized = TRUE;
    LOG_INFO("CanSM initialized, networks set to NOCOM");
}

void CanSM_DeInit(void) {
    if (!g_initialized) {
        return;
    }

    if (g_config != NULL) {
        for (uint8 i = 0U; i < g_config->numNetworks; i++) {
            NetworkHandleType net = g_config->networks[i].networkHandle;
            if ((net < CANSM_MAX_NETWORKS) && (g_networkState[net] == CANSM_FULLCOM)) {
                (void)CanIf_SetControllerMode(g_config->networks[i].canControllerId, CAN_CS_STOPPED);
            }
        }
    }

    for (uint8 i = 0U; i < CANSM_MAX_NETWORKS; i++) {
        g_networkState[i] = CANSM_UNINIT;
    }

    g_config = NULL;
    g_initialized = FALSE;
    LOG_INFO("CanSM de-initialized");
}

Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType ComM_Mode) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_CANSM, network, CANSM_API_ID_REQUEST_COMM_MODE, DET_ERR_UNINIT);
        return E_NOT_OK;
    }

    const CanSM_NetworkConfigType *netCfg = FindNetwork(network);
    if ((netCfg == NULL) || (network >= CANSM_MAX_NETWORKS)) {
        (void)Det_ReportError(DET_MODULE_ID_CANSM, network, CANSM_API_ID_REQUEST_COMM_MODE, DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    if (ComM_Mode == COMM_FULL_COMMUNICATION) {
        if (CanIf_SetControllerMode(netCfg->canControllerId, CAN_CS_STARTED) != E_OK) {
            LOG_ERROR("CanIf_SetControllerMode CAN_CS_STARTED failed for controller %u",
                      (unsigned)netCfg->canControllerId);
            return E_NOT_OK;
        }
        g_networkState[network] = CANSM_FULLCOM;
        LOG_INFO("network %u transitioned to CANSM_FULLCOM (controller %u STARTED)",
                 (unsigned)network, (unsigned)netCfg->canControllerId);
        return E_OK;
    } else if (ComM_Mode == COMM_NO_COMMUNICATION) {
        (void)CanIf_SetControllerMode(netCfg->canControllerId, CAN_CS_STOPPED);
        g_networkState[network] = CANSM_NOCOM;
        LOG_INFO("network %u transitioned to CANSM_NOCOM (controller %u STOPPED)",
                 (unsigned)network, (unsigned)netCfg->canControllerId);
        return E_OK;
    }

    return E_NOT_OK;
}

Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType *ComM_Mode) {
    if ((!g_initialized) || (ComM_Mode == NULL) || (network >= CANSM_MAX_NETWORKS)) {
        return E_NOT_OK;
    }

    if (g_networkState[network] == CANSM_FULLCOM) {
        *ComM_Mode = COMM_FULL_COMMUNICATION;
    } else {
        *ComM_Mode = COMM_NO_COMMUNICATION;
    }
    return E_OK;
}

CanSM_StateType CanSM_GetState(NetworkHandleType network) {
    if (network >= CANSM_MAX_NETWORKS) {
        return CANSM_UNINIT;
    }
    return g_networkState[network];
}

void CanSM_MainFunction(void) {
    if (!g_initialized) {
        return;
    }
    /* Periodic state machine transitions and bus-off checks */
}
