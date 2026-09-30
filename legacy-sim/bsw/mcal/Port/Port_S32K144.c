/**
 * @file Port_S32K144.c
 * @brief AUTOSAR Port driver implementation for NXP S32K144.
 *
 * Configures pin multiplexing:
 * - FlexCAN0: PTE4 (CAN0_RX, ALT5), PTE5 (CAN0_TX, ALT5)
 * - ADC0: PTB12 (ADC0_SE7, ALT0 analog input)
 * - GPIO Outputs: PTD0, PTD15, PTD16 (RGB LEDs on EVB, ALT1 GPIO)
 * - GPIO Inputs: PTC12 (SW2 Push Button, ALT1 GPIO + Pull-up)
 */

#include "Port.h"
#include "S32K144.h"

#include <stddef.h>

void Port_Init(const Port_ConfigType *config) {
    (void)config;

    /* 1. FlexCAN0 Pins (PTE4 = Rx, PTE5 = Tx) */
    PORTE->PCR[4] = PORT_PCR_MUX(5);   /* ALT5 = CAN0_RX */
    PORTE->PCR[5] = PORT_PCR_MUX(5);   /* ALT5 = CAN0_TX */

    /* 2. ADC0 Analog Input Pin (PTB12 = ADC0_SE7) */
    PORTB->PCR[12] = PORT_PCR_MUX(0);  /* ALT0 = Analog Input */

    /* 3. GPIO Pin: PTC12 (SW2 Button, active low) */
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PTC->PDDR &= ~(1UL << 12);         /* Input */

    /* 4. GPIO Pins: PTD0, PTD15, PTD16 (RGB LEDs) */
    PORTD->PCR[0]  = PORT_PCR_MUX(1);  /* PTD0  = Blue LED */
    PORTD->PCR[15] = PORT_PCR_MUX(1);  /* PTD15 = Red LED */
    PORTD->PCR[16] = PORT_PCR_MUX(1);  /* PTD16 = Green LED */

    PTD->PDDR |= (1UL << 0) | (1UL << 15) | (1UL << 16); /* Outputs */
    /* Set high (turn off active-low LEDs) */
    PTD->PSOR |= (1UL << 0) | (1UL << 15) | (1UL << 16);
}
