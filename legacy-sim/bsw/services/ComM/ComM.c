/**
 * @file ComM.c
 * @brief Communication Manager (ComM) implementation.
 */
#include "ComM.h"

#include <stddef.h>
#include <string.h>

#include "CanSM.h"
#include "Com.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "Logger.h"

#define LOG_INFO(...)  Logger_Log(LOGGER_LEVEL_INFO, "ComM", __VA_ARGS__)
#define LOG_WARN(...)  Logger_Log(LOGGER_LEVEL_WARN, "ComM", __VA_ARGS__)
#define LOG_ERROR(...) Logger_Log(LOGGER_LEVEL_ERROR, "ComM", __VA_ARGS__)

static const ComM_ConfigType *g_config;
static ComM_InitStatusType g_initStatus = COMM_UNINIT;
static ComM_ModeType g_userRequestedMode[COMM_MAX_USERS];
static ComM_ModeType g_channelMode[COMM_MAX_CHANNELS];

static void ComM_EvaluateChannel(NetworkHandleType channelId) {
    if (g_config == NULL) {
        return;
    }

    const ComM_ChannelConfigType *chCfg = NULL;
    for (uint8 i = 0U; i < g_config->numChannels; i++) {
        if (g_config->channels[i].channelId == channelId) {
            chCfg = &g_config->channels[i];
            break;
        }
    }

    if (chCfg == NULL) {
        return;
    }

    /* Mode arbitration: if any user mapped to this channel requests FULL_COMMUNICATION,
     * the channel must be in FULL_COMMUNICATION. */
    ComM_ModeType targetMode = COMM_NO_COMMUNICATION;
    for (uint8 u = 0U; u < chCfg->numUsers; u++) {
        ComM_UserHandleType user = chCfg->users[u];
        if (user < COMM_MAX_USERS) {
            if (g_userRequestedMode[user] == COMM_FULL_COMMUNICATION) {
                targetMode = COMM_FULL_COMMUNICATION;
                break;
            }
        }
    }

    ComM_ModeType currentMode = g_channelMode[channelId];
    if (targetMode != currentMode) {
        if (targetMode == COMM_FULL_COMMUNICATION) {
            /* 1. Request CanSM to start the CAN network */
            if (CanSM_RequestComMode(channelId, COMM_FULL_COMMUNICATION) == E_OK) {
                /* 2. Enable IPDU group communication in Com */
                Com_IpduGroupStart(COM_DEFAULT_IPDU_GROUP_ID, TRUE);
                g_channelMode[channelId] = COMM_FULL_COMMUNICATION;
                LOG_INFO("channel %u transitioned to COMM_FULL_COMMUNICATION", (unsigned)channelId);
            } else {
                LOG_ERROR("CanSM_RequestComMode FULL_COMMUNICATION failed for channel %u", (unsigned)channelId);
            }
        } else if (targetMode == COMM_NO_COMMUNICATION) {
            /* 1. Stop IPDU groups in Com */
            Com_IpduGroupStop(COM_DEFAULT_IPDU_GROUP_ID);
            /* 2. Request CanSM to stop the CAN network */
            (void)CanSM_RequestComMode(channelId, COMM_NO_COMMUNICATION);
            g_channelMode[channelId] = COMM_NO_COMMUNICATION;
            LOG_INFO("channel %u transitioned to COMM_NO_COMMUNICATION", (unsigned)channelId);
        }
    }
}

Std_ReturnType ComM_Init(const ComM_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_COMM, 0U, COMM_API_ID_INIT, DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }

    g_config = config;
    for (uint8 i = 0U; i < COMM_MAX_USERS; i++) {
        g_userRequestedMode[i] = COMM_NO_COMMUNICATION;
    }
    for (uint8 i = 0U; i < COMM_MAX_CHANNELS; i++) {
        g_channelMode[i] = COMM_NO_COMMUNICATION;
    }

    g_initStatus = COMM_INIT;
    LOG_INFO("ComM initialized");
    return E_OK;
}

void ComM_DeInit(void) {
    if (g_initStatus != COMM_INIT) {
        return;
    }

    for (uint8 i = 0U; i < COMM_MAX_CHANNELS; i++) {
        if (g_channelMode[i] == COMM_FULL_COMMUNICATION) {
            Com_IpduGroupStop(COM_DEFAULT_IPDU_GROUP_ID);
            (void)CanSM_RequestComMode(i, COMM_NO_COMMUNICATION);
            g_channelMode[i] = COMM_NO_COMMUNICATION;
        }
    }

    g_config = NULL;
    g_initStatus = COMM_UNINIT;
    LOG_INFO("ComM de-initialized");
}

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType user, ComM_ModeType mode) {
    if (g_initStatus != COMM_INIT) {
        (void)Det_ReportError(DET_MODULE_ID_COMM, user, COMM_API_ID_REQUEST_COMM_MODE, DET_ERR_UNINIT);
        return E_NOT_OK;
    }

    if (user >= COMM_MAX_USERS) {
        (void)Det_ReportError(DET_MODULE_ID_COMM, user, COMM_API_ID_REQUEST_COMM_MODE, DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    g_userRequestedMode[user] = mode;

    /* Evaluate all channels associated with this configuration */
    if (g_config != NULL) {
        for (uint8 i = 0U; i < g_config->numChannels; i++) {
            NetworkHandleType ch = g_config->channels[i].channelId;
            if (ch < COMM_MAX_CHANNELS) {
                ComM_EvaluateChannel(ch);
            }
        }
    }

    return E_OK;
}

Std_ReturnType ComM_GetMaxComMode(ComM_UserHandleType user, ComM_ModeType *mode) {
    if ((g_initStatus != COMM_INIT) || (mode == NULL) || (user >= COMM_MAX_USERS)) {
        return E_NOT_OK;
    }
    *mode = COMM_FULL_COMMUNICATION;
    return E_OK;
}

Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType user, ComM_ModeType *mode) {
    if ((g_initStatus != COMM_INIT) || (mode == NULL) || (user >= COMM_MAX_USERS)) {
        return E_NOT_OK;
    }
    *mode = g_userRequestedMode[user];
    return E_OK;
}

Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType user, ComM_ModeType *mode) {
    if ((g_initStatus != COMM_INIT) || (mode == NULL) || (user >= COMM_MAX_USERS)) {
        return E_NOT_OK;
    }

    /* Return mode of the primary channel */
    *mode = g_channelMode[COMM_CHANNEL_CAN];
    return E_OK;
}

ComM_ModeType ComM_GetChannelMode(NetworkHandleType channel) {
    if (channel >= COMM_MAX_CHANNELS) {
        return COMM_NO_COMMUNICATION;
    }
    return g_channelMode[channel];
}

void ComM_MainFunction(void) {
    if (g_initStatus != COMM_INIT) {
        return;
    }
}
