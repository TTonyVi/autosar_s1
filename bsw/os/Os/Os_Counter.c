#include "Os_Counter.h"

#include "Det.h"
#include "Det_ModuleIds.h"
#include "SysTimer.h" /* the OS's own tick source; never Gpt (section 8.1) */

#define OS_COUNTER_API_ID_INIT ((Det_ApiIdType)10U)
#define OS_COUNTER_API_ID_GET_VALUE ((Det_ApiIdType)11U)

static boolean g_initialized = FALSE;
static SysTimer_TickType g_originTicks = 0U;

void Os_Counter_Init(void) {
    /* Zero the counter against the current hardware tick, so the counter
     * starts at 0 regardless of how long SysTimer has been running. */
    g_originTicks = SysTimer_GetTicks();
    g_initialized = TRUE;
}

void Os_Counter_DeInit(void) {
    g_initialized = FALSE;
    g_originTicks = 0U;
}

Os_TickType Os_Counter_GetValue(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, OS_COUNTER_API_ID_GET_VALUE, DET_ERR_UNINIT);
        return 0U;
    }
    /* Unsigned wrap-around subtraction stays correct across the 2^32 tick
     * rollover, which is the standard counter idiom. */
    return (Os_TickType)(SysTimer_GetTicks() - g_originTicks);
}

Os_MilliType Os_Counter_TicksToMs(Os_TickType ticks) {
    /* SYSTIMER_TICKS_PER_MS is 1 on this host, so this is the identity
     * here; it is not on a real target. See SysTimer_Cfg.h. */
    return (Os_MilliType)(ticks / (Os_TickType)SYSTIMER_TICKS_PER_MS);
}
