#include "Gpt_Fake.h"

static Gpt_ValueType g_fakeNowMs = 0U;

void Gpt_Init(const Gpt_ConfigType *config) {
    (void)config; /* the fake needs no channel config */
    g_fakeNowMs = 0U;
}

void Gpt_DeInit(void) {
    /* nothing to release; kept for API parity with the real Gpt */
}

Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType channel) {
    (void)channel; /* the fake models one implicit time base, like GPT_CHANNEL_TIME_BASE */
    return g_fakeNowMs;
}

void GptFake_Reset(void) {
    g_fakeNowMs = 0U;
}

void GptFake_SetTimeMs(Gpt_ValueType ms) {
    g_fakeNowMs = ms;
}

void GptFake_AdvanceMs(Gpt_ValueType deltaMs) {
    g_fakeNowMs += deltaMs;
}
