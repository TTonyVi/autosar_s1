/**
 * @file SysTimer_Cfg.c
 * @brief The single static SysTimer configuration instance.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 11.2: configuration is
 * static, const, and file-scope — no runtime construction, no heap.
 */
#include "SysTimer.h"

const SysTimer_ConfigType SysTimer_Config = {
    .channelId = SYSTIMER_CHANNEL_OS,
    .ticksPerMs = SYSTIMER_TICKS_PER_MS,
};
