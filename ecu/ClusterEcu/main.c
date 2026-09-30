/**
 * @file main.c
 * @brief Bare-metal AUTOSAR entry point for NXP S32K144 (ClusterEcu).
 *
 * Demonstrates pure AUTOSAR execution on an embedded target:
 * - No POSIX dependencies (no signal.h, no CLI parsing).
 * - System boots from Reset_Handler -> main().
 * - EcuM initializes Mcu (80 MHz), Port, SysTick, FlexCAN, and the
 *   LPUART1-backed Display driver.
 * - StartOS() initializes BswM, transitions through its action lists,
 *   starts ComM/CanSM, and never returns.
 */

#include "ClusterEcu_Cfg.h"
#include "EcuM.h"
#include "Logger.h"

/* Static post-build configuration for S32K144 hardware. hwLinkPath is a
 * label only on this target — Display_S32K144.c always drives LPUART1,
 * it does not branch on this string the way the host VirtualHw path did. */
static const EcuM_PostBuildType g_postBuildS32K144 = {
    .canInterface = "CAN0",
    .hwLinkPath = "HARDWARE",
    .hwLinkEnabled = TRUE,
    .logLevel = LOGGER_LEVEL_INFO,
    .durationSeconds = 0U /* Run indefinitely */
};

/**
 * @brief S32K144 ClusterEcu firmware entry point.
 */
int main(void) {
    /* 1. Build link-time and post-build configuration for ClusterEcu */
    const EcuM_ConfigType *config = ClusterEcu_BuildConfig(&g_postBuildS32K144);
    if (config == NULL) {
        /* Trap on configuration error */
        while (1) {
            __asm volatile("wfi");
        }
    }

    /* 2. Execute AUTOSAR EcuM Phase 1 (Pre-OS) initialization, then
     * StartOS(OSDEFAULTAPPMODE). On success, StartOS() takes over CPU
     * control permanently and never returns! */
    if (EcuM_Init(config) != E_OK) {
        /* Startup failed: trigger shutdown sequence */
        EcuM_Shutdown();
        while (1) {
            __asm volatile("wfi");
        }
    }

    return 0;
}
