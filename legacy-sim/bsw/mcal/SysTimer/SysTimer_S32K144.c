/**
 * @file SysTimer_S32K144.c
 * @brief AUTOSAR SysTimer driver implementation for NXP S32K144.
 *
 * Configures ARM Cortex-M4 SysTick hardware timer for 1ms tick interrupts
 * running from the 80 MHz Core Clock.
 */

#include "SysTimer.h"
#include "S32K144.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define SYSTIMER_API_ID_INIT      ((Det_ApiIdType)1U)
#define SYSTIMER_API_ID_GET_TICKS ((Det_ApiIdType)2U)

static volatile SysTimer_TickType g_sysTimerTicks = 0U;
static boolean g_sysTimerInitialized = FALSE;

/**
 * @brief ARM Cortex-M4 SysTick Interrupt Handler.
 * Increments the 1ms tick counter.
 */
void SysTick_Handler(void) {
    g_sysTimerTicks++;
}

void SysTimer_Init(const SysTimer_ConfigType *config) {
    (void)config;

    /* S32K144 Core clock = 80 MHz.
     * SysTick reload value for 1 ms tick = (80,000,000 / 1,000) - 1 = 79,999.
     */
    SysTick->LOAD = 79999UL;
    SysTick->VAL  = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;

    g_sysTimerTicks = 0U;
    g_sysTimerInitialized = TRUE;
}

void SysTimer_DeInit(void) {
    SysTick->CTRL = 0UL;
    g_sysTimerInitialized = FALSE;
}

SysTimer_TickType SysTimer_GetTicks(void) {
    if (!g_sysTimerInitialized) {
        (void)Det_ReportError(DET_MODULE_ID_SYSTIMER, 0U, SYSTIMER_API_ID_GET_TICKS, DET_ERR_UNINIT);
        return 0U;
    }
    return g_sysTimerTicks;
}
