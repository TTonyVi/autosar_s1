/**
 * @file SysTimer.h
 * @brief Dedicated OS system timer driver (MCAL).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 7.4 and 8.1.
 *
 * `SysTimer` exists so the OS owns its own time source, exactly as in real
 * AUTOSAR: the OS is fed by a dedicated hardware timer (SysTick on
 * Cortex-M, STM on AURIX) that nothing else shares, while `Gpt` is the
 * general-purpose timer serving other BSW modules (COM deadlines, IoHwAb
 * staleness, Dem timestamps, Display write pacing). The two are separated
 * for safety, certification, and initialization-order reasons: the OS time
 * base must come up before any module that `Gpt` serves, and an
 * application-level misuse of `Gpt` must never be able to disturb
 * scheduling.
 *
 * On this Ubuntu host both drivers ultimately read `CLOCK_MONOTONIC`, but
 * they are two independently initialized, independently zeroed counters —
 * `SysTimer` keeps its own start timestamp and never calls into `Gpt`, and
 * `Gpt` never calls into `SysTimer`. Both live under `bsw/mcal/`, so
 * REQ-ARCH-009 (only MCAL may call `clock_gettime`) is unchanged.
 *
 * Only `Os_Counter` (bsw/os/Os/) is a legitimate caller of this module.
 */
#ifndef SYSTIMER_H
#define SYSTIMER_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "SysTimer_Cfg.h"

/** Free-running tick count since `SysTimer_Init`. Wraps at 2^32 ticks. */
typedef uint32 SysTimer_TickType;

typedef struct {
    /** Must be SYSTIMER_CHANNEL_OS; present so the config has the same
     *  shape as every other MCAL driver's config in this project. */
    uint8 channelId;
    /** Ticks per millisecond; must equal SYSTIMER_TICKS_PER_MS. */
    uint32 ticksPerMs;
} SysTimer_ConfigType;

/** The one static configuration instance (SysTimer_Cfg.c). */
extern const SysTimer_ConfigType SysTimer_Config;

/**
 * Starts the OS time base. A null or inconsistent config is reported to
 * Det but still starts the timer: a stopped OS time base would stop the
 * scheduler entirely, which is a worse failure than a misconfiguration.
 */
void SysTimer_Init(const SysTimer_ConfigType *config);

void SysTimer_DeInit(void);

/**
 * Ticks elapsed since `SysTimer_Init`. Returns 0 and reports a Det error
 * if called before initialization.
 */
SysTimer_TickType SysTimer_GetTicks(void);

#endif /* SYSTIMER_H */
