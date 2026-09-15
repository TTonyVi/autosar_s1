#include "Adc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "VirtualHw.h"

#define ADC_API_ID_INIT ((Det_ApiIdType)1U)
#define ADC_API_ID_READ_GROUP ((Det_ApiIdType)2U)
#define ADC_API_ID_GET_STATUS ((Det_ApiIdType)3U)
#define ADC_API_ID_GET_AGE ((Det_ApiIdType)4U)
#define ADC_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)5U)

#define ADC_MAX_GROUPS ((uint8)4U)

typedef struct {
    boolean configured;
    Adc_GroupConfigType config;
    uint32 readCount;
} Adc_GroupStateType;

static Adc_GroupStateType g_groups[ADC_MAX_GROUPS];
static boolean g_initialized;

static void BuildAdcKey(uint8 channelId, char *outKey, size_t outKeySize) {
    (void)snprintf(outKey, outKeySize, "ADC%u", (unsigned)channelId);
}

static boolean GroupIsValid(Adc_GroupType group) {
    return g_initialized && (group < ADC_MAX_GROUPS) && g_groups[group].configured;
}

void Adc_Init(const Adc_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, 0U, ADC_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if ((config->groups == NULL) || (config->numGroups == 0U) ||
        (config->numGroups > ADC_MAX_GROUPS)) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, 0U, ADC_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    (void)memset(g_groups, 0, sizeof(g_groups));
    for (uint8 i = 0U; i < config->numGroups; i++) {
        Adc_GroupType groupId = config->groups[i].groupId;
        if (groupId >= ADC_MAX_GROUPS) {
            (void)Det_ReportError(DET_MODULE_ID_ADC, i, ADC_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            continue;
        }
        g_groups[groupId].configured = TRUE;
        g_groups[groupId].config = config->groups[i];
    }
    g_initialized = TRUE;
}

void Adc_DeInit(void) {
    g_initialized = FALSE;
}

Std_ReturnType Adc_ReadGroup(Adc_GroupType group, Adc_ValueGroupType *data_buffer) {
    if (data_buffer == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, group, ADC_API_ID_READ_GROUP,
                              DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }
    if (!GroupIsValid(group)) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, group, ADC_API_ID_READ_GROUP,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType ageMs;
    boolean isFresh;
    if (!VirtualHw_GetLatestPayload(g_groups[group].config.virtualHwLinkId, payload,
                                    sizeof(payload), &ageMs, &isFresh)) {
        return E_NOT_OK; /* no conversion result has ever arrived */
    }

    char key[8];
    BuildAdcKey(g_groups[group].config.channelId, key, sizeof(key));
    uint32 value;
    if (VirtualHw_ParseUint(payload, key, &value) != E_OK) {
        return E_NOT_OK;
    }

    /* Raw pass-through: no scaling, clamping, or interpretation
     * (REQ-MCAL-001), even if `value` exceeds the channel's bit
     * resolution — plausibility is IoHwAb's judgment (Phase 5), not this
     * module's. */
    *data_buffer = (Adc_ValueGroupType)value;
    g_groups[group].readCount++;
    return E_OK;
}

Adc_StatusType Adc_GetGroupStatus(Adc_GroupType group) {
    if (!GroupIsValid(group)) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, group, ADC_API_ID_GET_STATUS,
                              DET_ERR_INVALID_PARAM);
        return ADC_IDLE;
    }

    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType ageMs;
    boolean isFresh;
    boolean hasAny = VirtualHw_GetLatestPayload(g_groups[group].config.virtualHwLinkId, payload,
                                                sizeof(payload), &ageMs, &isFresh);
    return hasAny ? ADC_STREAM_CONVERTED : ADC_IDLE;
}

Gpt_ValueType Adc_GetGroupAge(Adc_GroupType group) {
    if (!GroupIsValid(group)) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, group, ADC_API_ID_GET_AGE, DET_ERR_INVALID_PARAM);
        return UINT32_MAX;
    }

    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType ageMs;
    boolean isFresh;
    if (!VirtualHw_GetLatestPayload(g_groups[group].config.virtualHwLinkId, payload,
                                    sizeof(payload), &ageMs, &isFresh)) {
        return UINT32_MAX;
    }
    return ageMs;
}

uint32 Adc_GetGroupReadCount(Adc_GroupType group) {
    return GroupIsValid(group) ? g_groups[group].readCount : 0U;
}

void Adc_MainFunction_Read(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_ADC, 0U, ADC_API_ID_MAIN_FUNCTION_READ, DET_ERR_UNINIT);
        return;
    }
    for (uint8 g = 0U; g < ADC_MAX_GROUPS; g++) {
        if (g_groups[g].configured) {
            /* Draining is idempotent within one activation: if Dio also
             * shares this link and drains it moments later in the same
             * tick, VirtualHw simply finds nothing new. */
            VirtualHw_MainFunction_Read(g_groups[g].config.virtualHwLinkId);
        }
    }
}
