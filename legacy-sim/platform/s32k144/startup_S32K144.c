/**
 * @file startup_S32K144.c
 * @brief Startup code, Vector Table, and Reset Handler for NXP S32K144.
 */

#include "S32K144.h"

/* Linker symbols */
extern uint32 _etext;
extern uint32 __data_start__;
extern uint32 __data_end__;
extern uint32 __bss_start__;
extern uint32 __bss_end__;
extern uint32 __StackTop;

/* External entry point */
extern int main(void);

/* Forward declarations of core exception handlers */
void Reset_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* Peripheral handlers (weak defaults) */
void CAN0_ORed_0_15_MB_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void ADC0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

/* Default catch-all handler */
void Default_Handler(void) {
    while (1) {
        __asm volatile("wfi");
    }
}

/* Flash Configuration Field (at 0x0000_0400) */
__attribute__((section(".flash_config"), used))
const uint8 g_flashConfig[16] = {
    0xFFU, 0xFFU, 0xFFU, 0xFFU, /* Backdoor Comparison Key 0-3 */
    0xFFU, 0xFFU, 0xFFU, 0xFFU, /* Backdoor Comparison Key 4-7 */
    0xFFU, 0xFFU, 0xFFU, 0xFFU, /* Program flash protection bytes */
    0xFEU,                      /* Flash security byte (unsecured) */
    0xFFU,                      /* Flash nonvolatile option byte */
    0xFFU,                      /* EEPROM protection byte */
    0xFFU                       /* Data flash protection byte */
};

/* Vector table in Flash at 0x0000_0000 */
typedef void (*const ExecFuncPtr)(void);

__attribute__((section(".isr_vector"), used))
const ExecFuncPtr __vector_table[] = {
    (ExecFuncPtr)&__StackTop,           /* Initial Stack Pointer */
    Reset_Handler,                      /* Reset Handler */
    NMI_Handler,                        /* NMI Handler */
    HardFault_Handler,                  /* Hard Fault Handler */
    MemManage_Handler,                  /* MPU Fault Handler */
    BusFault_Handler,                   /* Bus Fault Handler */
    UsageFault_Handler,                 /* Usage Fault Handler */
    0,                                  /* Reserved */
    0,                                  /* Reserved */
    0,                                  /* Reserved */
    0,                                  /* Reserved */
    SVC_Handler,                        /* SVCall Handler */
    DebugMon_Handler,                   /* Debug Monitor Handler */
    0,                                  /* Reserved */
    PendSV_Handler,                     /* PendSV Handler */
    SysTick_Handler,                    /* SysTick Handler */

    /* External Interrupts (subset for S32K144 AUTOSAR demo) */
    Default_Handler,                    /* 0: DMA channel 0 */
    Default_Handler,                    /* 1: DMA channel 1 */
    Default_Handler,                    /* 2: DMA channel 2 */
    Default_Handler,                    /* 3: DMA channel 3 */
    ADC0_IRQHandler,                    /* 39: ADC0 conversion complete */
    CAN0_ORed_0_15_MB_IRQHandler        /* 81: CAN0 MB 0-15 */
};

/**
 * @brief Disable Watchdog Timer (WDOG).
 * Must unlock within 256 bus cycles of reset.
 */
static void WDOG_Disable(void) {
    /* Unlock WDOG */
    WDOG->CNT = WDOG_UNLOCK_KEY;
    /* Wait until unlocked */
    while ((WDOG->CS & WDOG_CS_UPDATE_MASK) == 0U) {
    }
    /* Set timeout to max and clear enable bit */
    WDOG->TOVAL = 0xFFFFU;
    WDOG->CS = (uint32)((WDOG->CS & ~WDOG_CS_EN_MASK) | WDOG_CS_UPDATE_MASK);
}

/**
 * @brief S32K144 Reset Handler.
 * Initializes hardware registers, copies data, zeroes bss, calls main().
 */
void Reset_Handler(void) {
    /* 1. Disable Watchdog to avoid resets during init */
    WDOG_Disable();

    /* 2. Copy .data section from Flash to SRAM */
    uint32 *src = &_etext;
    uint32 *dst = &__data_start__;
    while (dst < &__data_end__) {
        *dst++ = *src++;
    }

    /* 3. Clear .bss section in SRAM */
    dst = &__bss_start__;
    while (dst < &__bss_end__) {
        *dst++ = 0U;
    }

    /* 4. Jump to main() */
    (void)main();

    /* 5. In case main() returns, sleep */
    while (1) {
        __asm volatile("wfi");
    }
}
