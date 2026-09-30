/**
 * @file Mcu_S32K144.c
 * @brief AUTOSAR Mcu driver implementation for NXP S32K144.
 *
 * Configures System Clock (80 MHz Core, 40 MHz Bus, 26.67 MHz Flash)
 * using 8 MHz External Crystal (SOSC) and System PLL (SPLL), and enables
 * Peripheral Clock Control (PCC) clock gating for required modules.
 */

#include "Mcu.h"
#include "S32K144.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define MCU_API_ID_INIT        ((Det_ApiIdType)1U)
#define MCU_API_ID_INIT_CLOCK  ((Det_ApiIdType)2U)

static boolean g_mcuInitialized = FALSE;

/**
 * @brief Initialize SOSC 8MHz and SPLL 80MHz clock tree.
 */
static void Mcu_Hw_InitClocks(void) {
    /* 1. Initialize SOSC (8 MHz External Crystal) */
    SCG->SOSCDIV = 0x00010101UL;       /* SOSCDIV1=1, SOSCDIV2=1 */
    SCG->SOSCCFG = 0x00000024UL;       /* Medium frequency range (4-8 MHz), high gain, external crystal */
    while ((SCG->SOSCCSR & (1UL << 23)) != 0U) {
        /* Wait if SOSCCSR is locked */
    }
    SCG->SOSCCSR = SCG_SOSCCSR_SOSCEN_MASK; /* Enable SOSC */
    while ((SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) == 0U) {
        /* Wait for SOSC to become valid */
    }

    /* 2. Configure System PLL (SPLL) to 160 MHz VCO, divided to 80 MHz Core */
    while ((SCG->SPLLCSR & (1UL << 23)) != 0U) {
        /* Wait if SPLLCSR is locked */
    }
    SCG->SPLLCSR = 0U;                 /* Disable SPLL before configuration */
    SCG->SPLLDIV = 0x00010101UL;       /* SPLLDIV1=1, SPLLDIV2=1 */
    SCG->SPLLCFG = 0x00040000UL;       /* PREDIV=0 (/1), MULT=4 (*20): 8MHz * 20 = 160MHz VCO */
    SCG->SPLLCSR = SCG_SPLLCSR_SPLLEN_MASK; /* Enable SPLL */
    while ((SCG->SPLLCSR & SCG_SPLLCSR_SPLLVLD_MASK) == 0U) {
        /* Wait for SPLL to lock */
    }

    /* 3. Switch Run Clock (RCCR) to SPLL */
    /* SCS = 6 (SPLL), DIVCORE = 1 (/2 -> 80MHz), DIVBUS = 1 (/2 -> 40MHz), DIVSLOW = 2 (/3 -> 26.67MHz) */
    SCG->RCCR = 0x06010012UL;
    while (((SCG->CSR >> 24) & 0x0FUL) != 6U) {
        /* Wait until system clock source is SPLL */
    }
}

/**
 * @brief Enable clocks to peripherals via PCC.
 */
static void Mcu_Hw_InitPeripheralClocks(void) {
    /* Enable GPIO Port clocks */
    PCC->PCC[PCC_PORTA_INDEX] |= PCC_CGC_MASK;
    PCC->PCC[PCC_PORTB_INDEX] |= PCC_CGC_MASK;
    PCC->PCC[PCC_PORTC_INDEX] |= PCC_CGC_MASK;
    PCC->PCC[PCC_PORTD_INDEX] |= PCC_CGC_MASK;
    PCC->PCC[PCC_PORTE_INDEX] |= PCC_CGC_MASK;

    /* Enable FlexCAN0 clock */
    PCC->PCC[PCC_FlexCAN0_INDEX] |= PCC_CGC_MASK;

    /* Enable ADC0 clock with SOSCDIV2 (4MHz) */
    PCC->PCC[PCC_ADC0_INDEX] = PCC_CGC_MASK | (1UL << PCC_PCS_SHIFT);
}

void Mcu_Init(const Mcu_ConfigType *config) {
    (void)config;

    Mcu_Hw_InitClocks();
    Mcu_Hw_InitPeripheralClocks();

    g_mcuInitialized = TRUE;
}

Std_ReturnType Mcu_InitClock(Mcu_ClockType clock_setting) {
    (void)clock_setting;
    if (!g_mcuInitialized) {
        (void)Det_ReportError(DET_MODULE_ID_MCU, 0U, MCU_API_ID_INIT_CLOCK, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    return E_OK;
}
