#include "Adc_Fake.h"

#include <stddef.h>

#define ADC_FAKE_MAX_GROUPS 4U

static Adc_ValueGroupType g_counts[ADC_FAKE_MAX_GROUPS];
static Gpt_ValueType g_ageMs[ADC_FAKE_MAX_GROUPS];
static Adc_StatusType g_status[ADC_FAKE_MAX_GROUPS];

void Adc_Init(const Adc_ConfigType *config) {
    (void)config;
    AdcFake_Reset();
}

void Adc_DeInit(void) {
    AdcFake_Reset();
}

Std_ReturnType Adc_ReadGroup(Adc_GroupType group, Adc_ValueGroupType *data_buffer) {
    if ((data_buffer == NULL) || (group >= (Adc_GroupType)ADC_FAKE_MAX_GROUPS)) {
        return E_NOT_OK;
    }
    *data_buffer = g_counts[group];
    return E_OK;
}

Adc_StatusType Adc_GetGroupStatus(Adc_GroupType group) {
    return (group < (Adc_GroupType)ADC_FAKE_MAX_GROUPS) ? g_status[group] : ADC_IDLE;
}

void Adc_MainFunction_Read(void) {
    /* nothing to drain in the fake; values are staged directly */
}

Gpt_ValueType Adc_GetGroupAge(Adc_GroupType group) {
    return (group < (Adc_GroupType)ADC_FAKE_MAX_GROUPS) ? g_ageMs[group] : 0U;
}

void AdcFake_Reset(void) {
    for (uint8 i = 0U; i < (uint8)ADC_FAKE_MAX_GROUPS; i++) {
        g_counts[i] = 0U;
        g_ageMs[i] = 0U;
        g_status[i] = ADC_IDLE;
    }
}

void AdcFake_SetGroupValue(Adc_GroupType group, Adc_ValueGroupType counts) {
    if (group < (Adc_GroupType)ADC_FAKE_MAX_GROUPS) {
        g_counts[group] = counts;
        g_status[group] = ADC_STREAM_CONVERTED;
    }
}

void AdcFake_SetGroupAge(Adc_GroupType group, Gpt_ValueType ageMs) {
    if (group < (Adc_GroupType)ADC_FAKE_MAX_GROUPS) {
        g_ageMs[group] = ageMs;
    }
}

void AdcFake_SetGroupIdle(Adc_GroupType group) {
    if (group < (Adc_GroupType)ADC_FAKE_MAX_GROUPS) {
        g_status[group] = ADC_IDLE;
        g_counts[group] = 0U;
    }
}
