#include "Dio_Fake.h"

#define DIO_FAKE_MAX_CHANNELS 4U

static Dio_LevelType g_level[DIO_FAKE_MAX_CHANNELS];
static Gpt_ValueType g_ageMs[DIO_FAKE_MAX_CHANNELS];

void Dio_Init(const Dio_ConfigType *config) {
    (void)config;
    DioFake_Reset();
}

void Dio_DeInit(void) {
    DioFake_Reset();
}

Dio_LevelType Dio_ReadChannel(Dio_ChannelType channel_id) {
    return (channel_id < (Dio_ChannelType)DIO_FAKE_MAX_CHANNELS) ? g_level[channel_id] : STD_LOW;
}

void Dio_MainFunction_Read(void) {
    /* nothing to drain in the fake */
}

Gpt_ValueType Dio_GetChannelAge(Dio_ChannelType channel_id) {
    return (channel_id < (Dio_ChannelType)DIO_FAKE_MAX_CHANNELS) ? g_ageMs[channel_id] : 0U;
}

void DioFake_Reset(void) {
    for (uint8 i = 0U; i < (uint8)DIO_FAKE_MAX_CHANNELS; i++) {
        g_level[i] = STD_LOW;
        /* Matches the real Dio: nothing received yet reports UINT32_MAX,
         * which is what makes "never sampled" read as stale. */
        g_ageMs[i] = 0xFFFFFFFFU;
    }
}

void DioFake_SetChannelLevel(Dio_ChannelType channel, Dio_LevelType level) {
    if (channel < (Dio_ChannelType)DIO_FAKE_MAX_CHANNELS) {
        g_level[channel] = level;
    }
}

void DioFake_SetChannelAge(Dio_ChannelType channel, Gpt_ValueType ageMs) {
    if (channel < (Dio_ChannelType)DIO_FAKE_MAX_CHANNELS) {
        g_ageMs[channel] = ageMs;
    }
}
