/**
 * @file main_s32k144.c
 * @brief Bare-Metal AUTOSAR Entry Point for NXP S32K144 (SensorEcu).
 *
 * Demonstrates pure AUTOSAR execution on an embedded target:
 * - No POSIX dependencies (no signal.h, unistd.h, sockets).
 * - System boots from Reset_Handler -> main().
 * - EcuM initializes MCU (80MHz), Pins, SysTick, FlexCAN, ADC.
 * - StartOS() initializes BswM, transitions through Action Lists, starts ComM/CanSM.
 * - Enters non-returning low-power scheduling loop (Wait-For-Interrupt: WFI).
 */

#include "EcuM.h"
#include "Gpt.h"
#include "Logger.h"
#include "SensorEcu_Cfg.h"

/* Static Post-Build Configuration for S32K144 hardware */
static const EcuM_PostBuildType g_postBuildS32K144 = {
    .canInterface = "CAN0",
    .hwLinkPath = "HARDWARE",
    .hwLinkEnabled = TRUE,
    .logLevel = LOGGER_LEVEL_INFO,
    .durationSeconds = 0U /* Run indefinitely */
};

/**
 * @brief S32K144 SensorEcu Firmware Entry Point.
 */
int main(void) {
    /* 1. Build link-time and post-build configuration for SensorEcu */
    const EcuM_ConfigType *config = SensorEcu_BuildConfig(&g_postBuildS32K144);
    if (config == NULL) {
        /* Trap on configuration error */
        while (1) {
            __asm volatile("wfi");
        }
    }

    /* 2. Execute AUTOSAR EcuM Phase 1 (Pre-OS) Initialization.
     * Initializes Mcu (80 MHz clock, PCC), Port (pin muxing), Det, SysTimer (SysTick),
     * Gpt (LPIT0), Can (FlexCAN0), and invokes StartOS(OSDEFAULTAPPMODE).
     * On success, StartOS() takes over CPU control permanently and never returns!
     */
    if (EcuM_Init(config) != E_OK) {
        /* Startup failed: trigger shutdown sequence */
        EcuM_Shutdown();
        while (1) {
            __asm volatile("wfi");
        }
    }

    return 0;
}
