#include "SysTimer_Fake.h"

static SysTimer_TickType g_fakeTicks = 0U;
static boolean g_initialized = FALSE;

void SysTimer_Init(const SysTimer_ConfigType *config) {
    (void)config; /* the fake needs no channel/rate programming */
    g_fakeTicks = 0U;
    g_initialized = TRUE;
}

void SysTimer_DeInit(void) {
    g_initialized = FALSE;
}

SysTimer_TickType SysTimer_GetTicks(void) {
    if (!g_initialized) {
        return 0U;
    }
    return g_fakeTicks;
}

void SysTimerFake_Reset(void) {
    g_fakeTicks = 0U;
    g_initialized = FALSE;
}

void SysTimerFake_SetTicks(SysTimer_TickType ticks) {
    g_fakeTicks = ticks;
}

void SysTimerFake_AdvanceTicks(SysTimer_TickType deltaTicks) {
    g_fakeTicks += deltaTicks;
}
