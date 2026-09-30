#include "Os_Counter.h"

#include "Det.h"
#include "Det_ModuleIds.h"
#include "SysTimer.h" /* the OS's own tick source; never Gpt (section 8.1) */

#define OS_COUNTER_API_ID_INIT ((Det_ApiIdType)10U)
#define OS_COUNTER_API_ID_GET_VALUE ((Det_ApiIdType)11U)

static boolean g_initialized = FALSE;
static SysTimer_TickType g_originTicks = 0U;

/* Vai trò: khởi tạo counter của Os — lưu lại "mốc gốc" bằng tick hiện
 * tại của SysTimer, để counter luôn bắt đầu từ 0 dù SysTimer đã chạy bao
 * lâu trước đó. */
void Os_Counter_Init(void) {
    /* Zero the counter against the current hardware tick, so the counter
     * starts at 0 regardless of how long SysTimer has been running. */
    g_originTicks = SysTimer_GetTicks();
    g_initialized = TRUE;
}

/* Vai trò: tắt counter (hạ cờ init, xoá mốc gốc). */
void Os_Counter_DeInit(void) {
    g_initialized = FALSE;
    g_originTicks = 0U;
}

/* Vai trò: đọc số tick đã trôi qua kể từ Os_Counter_Init — nguồn thời
 * gian DUY NHẤT mà Os.c (scheduler) được phép dùng. */
Os_TickType Os_Counter_GetValue(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, OS_COUNTER_API_ID_GET_VALUE, DET_ERR_UNINIT);
        return 0U;
    }
    /* Unsigned wrap-around subtraction stays correct across the 2^32 tick
     * rollover, which is the standard counter idiom. */
    return (Os_TickType)(SysTimer_GetTicks() - g_originTicks);
}

/* Vai trò: quy đổi số tick sang mili-giây (chia cho SYSTIMER_TICKS_PER_MS
 * — trên Ubuntu bằng 1 nên là phép identity, nhưng trên chip thật tỉ lệ
 * này có thể khác 1). */
Os_MilliType Os_Counter_TicksToMs(Os_TickType ticks) {
    /* SYSTIMER_TICKS_PER_MS is 1 on this host, so this is the identity
     * here; it is not on a real target. See SysTimer_Cfg.h. */
    return (Os_MilliType)(ticks / (Os_TickType)SYSTIMER_TICKS_PER_MS);
}
