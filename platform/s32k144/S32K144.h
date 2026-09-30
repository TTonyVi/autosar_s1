/**
 * @file S32K144.h
 * @brief Hardware Register Definitions for NXP S32K144 (ARM Cortex-M4F).
 *
 * Microcontroller Specifications:
 * - Core: ARM Cortex-M4F up to 80 MHz (RUN) / 112 MHz (HSRUN)
 * - Flash: 512 KB (0x0000_0000 - 0x0007_FFFF)
 * - SRAM: 64 KB (SRAM_L: 0x1FFF_8000 - 0x1FFF_FFFF; SRAM_U: 0x2000_0000 - 0x2000_7FFF)
 */

#ifndef S32K144_H
#define S32K144_H

#include "Platform_Types.h"

#define __IO volatile
#define __I  volatile const
#define __O  volatile

/* ============================================================================
 * Cortex-M4 Core Peripherals: SysTick, NVIC, SCB
 * ============================================================================ */
typedef struct {
    __IO uint32 CTRL;                   /* 0x00: SysTick Control and Status Register */
    __IO uint32 LOAD;                   /* 0x04: SysTick Reload Value Register */
    __IO uint32 VAL;                    /* 0x08: SysTick Current Value Register */
    __I  uint32 CALIB;                  /* 0x0C: SysTick Calibration Register */
} SysTick_Type;

#define SysTick_BASE                    (0xE000E010UL)
#define SysTick                         ((SysTick_Type *)SysTick_BASE)

#define SysTick_CTRL_ENABLE_Msk         (1UL << 0)
#define SysTick_CTRL_TICKINT_Msk        (1UL << 1)
#define SysTick_CTRL_CLKSOURCE_Msk      (1UL << 2)
#define SysTick_CTRL_COUNTFLAG_Msk      (1UL << 16)

/* ============================================================================
 * Watchdog Timer (WDOG)
 * ============================================================================ */
typedef struct {
    __IO uint32 CS;                     /* 0x00: Watchdog Control and Status Register */
    __IO uint32 CNT;                    /* 0x04: Watchdog Counter Register */
    __IO uint32 TOVAL;                  /* 0x08: Watchdog Timeout Value Register */
    __IO uint32 WIN;                    /* 0x0C: Watchdog Window Register */
} WDOG_Type;

#define WDOG_BASE                       (0x40052000UL)
#define WDOG                            ((WDOG_Type *)WDOG_BASE)

#define WDOG_CS_EN_MASK                 (1UL << 7)
#define WDOG_CS_UPDATE_MASK             (1UL << 5)
#define WDOG_CS_CLK_MASK                (3UL << 8)
#define WDOG_UNLOCK_KEY                 (0xD928C520UL)

/* ============================================================================
 * System Clock Generator (SCG)
 * ============================================================================ */
typedef struct {
    __I  uint32 VERID;                  /* 0x000: Version ID */
    __I  uint32 PARAM;                  /* 0x004: Parameter */
    uint32 RESERVED_0[2];
    __I  uint32 CSR;                    /* 0x010: Clock Status Register */
    __IO uint32 RCCR;                   /* 0x014: Run Clock Control Register */
    __IO uint32 VCCR;                   /* 0x018: VLPR Clock Control Register */
    __IO uint32 HCCR;                   /* 0x01C: HSRUN Clock Control Register */
    __IO uint32 CLKOUTCNFG;             /* 0x020: SCG CLKOUT Configuration Register */
    uint32 RESERVED_1[55];
    __IO uint32 SOSCCSR;                /* 0x100: System OSC Control Status Register */
    __IO uint32 SOSCDIV;                /* 0x104: System OSC Divide Register */
    __IO uint32 SOSCCFG;                /* 0x108: System OSC Configuration Register */
    uint32 RESERVED_2[61];
    __IO uint32 SIRCCSR;                /* 0x200: Slow IRC Control Status Register */
    __IO uint32 SIRCDIV;                /* 0x204: Slow IRC Divide Register */
    __IO uint32 SIRCCFG;                /* 0x208: Slow IRC Configuration Register */
    uint32 RESERVED_3[61];
    __IO uint32 FIRCCSR;                /* 0x300: Fast IRC Control Status Register */
    __IO uint32 FIRCDIV;                /* 0x304: Fast IRC Divide Register */
    __IO uint32 FIRCCFG;                /* 0x308: Fast IRC Configuration Register */
    uint32 RESERVED_4[189];
    __IO uint32 SPLLCSR;                /* 0x600: System PLL Control Status Register */
    __IO uint32 SPLLDIV;                /* 0x604: System PLL Divide Register */
    __IO uint32 SPLLCFG;                /* 0x608: System PLL Configuration Register */
} SCG_Type;

#define SCG_BASE                        (0x40064000UL)
#define SCG                             ((SCG_Type *)SCG_BASE)

/* SCG bit masks */
#define SCG_SOSCCSR_SOSCEN_MASK         (1UL << 0)
#define SCG_SOSCCSR_SOSCVLD_MASK        (1UL << 24)
#define SCG_SPLLCSR_SPLLEN_MASK         (1UL << 0)
#define SCG_SPLLCSR_SPLLVLD_MASK        (1UL << 24)

/* ============================================================================
 * Peripheral Clock Control (PCC)
 * ============================================================================ */
typedef struct {
    __IO uint32 PCC[116];               /* 0x000 - 0x1CC: Peripheral Clock Control Registers */
} PCC_Type;

#define PCC_BASE                        (0x40065000UL)
#define PCC                             ((PCC_Type *)PCC_BASE)

#define PCC_CGC_MASK                    (1UL << 30)
#define PCC_PCS_MASK                    (7UL << 24)
#define PCC_PCS_SHIFT                   (24U)

/* Peripheral Indices in PCC */
#define PCC_FlexCAN0_INDEX              (36U)
#define PCC_LPUART1_INDEX               (43U)
#define PCC_LPIT_INDEX                  (48U)
#define PCC_ADC0_INDEX                  (59U)
#define PCC_PORTA_INDEX                 (73U)
#define PCC_PORTB_INDEX                 (74U)
#define PCC_PORTC_INDEX                 (75U)
#define PCC_PORTD_INDEX                 (76U)
#define PCC_PORTE_INDEX                 (77U)

/* ============================================================================
 * Port Control and Interrupts (PORT)
 * ============================================================================ */
typedef struct {
    __IO uint32 PCR[32];                /* 0x000 - 0x07C: Pin Control Registers */
    __O  uint32 GPCHR;                  /* 0x080: Global Pin Control High Register */
    __O  uint32 GPCLR;                  /* 0x084: Global Pin Control Low Register */
    uint32 RESERVED_0[6];
    __IO uint32 ISFR;                   /* 0x0A0: Interrupt Status Flag Register */
    uint32 RESERVED_1[7];
    __IO uint32 DFER;                   /* 0x0C0: Digital Filter Enable Register */
    __IO uint32 DFCR;                   /* 0x0C4: Digital Filter Clock Register */
    __IO uint32 DFWR;                   /* 0x0C8: Digital Filter Width Register */
} PORT_Type;

#define PORTA_BASE                      (0x40049000UL)
#define PORTB_BASE                      (0x4004A000UL)
#define PORTC_BASE                      (0x4004B000UL)
#define PORTD_BASE                      (0x4004C000UL)
#define PORTE_BASE                      (0x4004D000UL)

#define PORTA                           ((PORT_Type *)PORTA_BASE)
#define PORTB                           ((PORT_Type *)PORTB_BASE)
#define PORTC                           ((PORT_Type *)PORTC_BASE)
#define PORTD                           ((PORT_Type *)PORTD_BASE)
#define PORTE                           ((PORT_Type *)PORTE_BASE)

/* PORT PCR bit masks */
#define PORT_PCR_MUX_MASK               (7UL << 8)
#define PORT_PCR_MUX(x)                 (((uint32)(x) << 8) & PORT_PCR_MUX_MASK)
#define PORT_PCR_PE_MASK                (1UL << 1)
#define PORT_PCR_PS_MASK                (1UL << 0)

/* ============================================================================
 * General Purpose Input/Output (GPIO)
 * ============================================================================ */
typedef struct {
    __IO uint32 PDOR;                   /* 0x00: Port Data Output Register */
    __O  uint32 PSOR;                   /* 0x04: Port Set Output Register */
    __O  uint32 PCOR;                   /* 0x08: Port Clear Output Register */
    __O  uint32 PTOR;                   /* 0x0C: Port Toggle Output Register */
    __I  uint32 PDIR;                   /* 0x10: Port Data Input Register */
    __IO uint32 PDDR;                   /* 0x14: Port Data Direction Register */
    __IO uint32 PIDR;                   /* 0x18: Port Input Disable Register */
} GPIO_Type;

#define PTA_BASE                        (0x400FF000UL)
#define PTB_BASE                        (0x400FF040UL)
#define PTC_BASE                        (0x400FF080UL)
#define PTD_BASE                        (0x400FF0C0UL)
#define PTE_BASE                        (0x400FF100UL)

#define PTA                             ((GPIO_Type *)PTA_BASE)
#define PTB                             ((GPIO_Type *)PTB_BASE)
#define PTC                             ((GPIO_Type *)PTC_BASE)
#define PTD                             ((GPIO_Type *)PTD_BASE)
#define PTE                             ((GPIO_Type *)PTE_BASE)

/* ============================================================================
 * FlexCAN (CAN Controller)
 * ============================================================================ */
typedef struct {
    __IO uint32 CS;                     /* Message Buffer Code & Status */
    __IO uint32 ID;                     /* Message Buffer Identifier */
    __IO uint32 WORD0;                  /* Data Bytes 0-3 */
    __IO uint32 WORD1;                  /* Data Bytes 4-7 */
} FlexCAN_MsgBuf_Type;

typedef struct {
    __IO uint32 MCR;                    /* 0x000: Module Configuration Register */
    __IO uint32 CTRL1;                  /* 0x004: Control 1 Register */
    __IO uint32 TIMER;                  /* 0x008: Free Running Timer */
    uint32 RESERVED_0;
    __IO uint32 RXMGMASK;               /* 0x010: Rx Mailboxes Global Mask */
    __IO uint32 RX14MASK;               /* 0x014: Rx 14 Mask */
    __IO uint32 RX15MASK;               /* 0x018: Rx 15 Mask */
    __IO uint32 ECR;                    /* 0x01C: Error Counter */
    __IO uint32 ESR1;                   /* 0x020: Error and Status 1 Register */
    uint32 RESERVED_1;
    __IO uint32 IMASK1;                 /* 0x028: Interrupt Masks 1 Register */
    uint32 RESERVED_2;
    __IO uint32 IFLAG1;                 /* 0x030: Interrupt Flags 1 Register */
    __IO uint32 CTRL2;                  /* 0x034: Control 2 Register */
    __I  uint32 ESR2;                   /* 0x038: Error and Status 2 Register */
    uint32 RESERVED_3[2];
    __I  uint32 CRCR;                   /* 0x044: CRC Register */
    __IO uint32 RXFGMASK;               /* 0x048: Rx FIFO Global Mask */
    __I  uint32 RXFIR;                  /* 0x04C: Rx FIFO Information Register */
    uint32 RESERVED_4[12];
    FlexCAN_MsgBuf_Type MB[32];         /* 0x080: Message Buffers 0-31 */
} FlexCAN_Type;

#define CAN0_BASE                       (0x40024000UL)
#define CAN0                            ((FlexCAN_Type *)CAN0_BASE)

/* FlexCAN MCR bit masks */
#define FlexCAN_MCR_MDIS_MASK           (1UL << 31)
#define FlexCAN_MCR_FRZ_MASK            (1UL << 30)
#define FlexCAN_MCR_HALT_MASK           (1UL << 28)
#define FlexCAN_MCR_NOTRDY_MASK         (1UL << 27)
#define FlexCAN_MCR_SOFTRST_MASK        (1UL << 25)
#define FlexCAN_MCR_FRZACK_MASK         (1UL << 24)
#define FlexCAN_MCR_MAXMB(x)            (((uint32)(x)) & 0x7FUL)

/* FlexCAN MB CS bit definitions */
#define FlexCAN_MB_CS_CODE_MASK         (0x0FUL << 24)
#define FlexCAN_MB_CS_CODE(x)           (((uint32)(x) << 24) & FlexCAN_MB_CS_CODE_MASK)
#define FlexCAN_MB_CS_SRR_MASK          (1UL << 22)
#define FlexCAN_MB_CS_IDE_MASK          (1UL << 21)
#define FlexCAN_MB_CS_RTR_MASK          (1UL << 20)
#define FlexCAN_MB_CS_DLC_MASK          (0x0FUL << 16)
#define FlexCAN_MB_CS_DLC(x)            (((uint32)(x) << 16) & FlexCAN_MB_CS_DLC_MASK)

/* FlexCAN MB Codes */
#define FlexCAN_MB_CODE_RX_INACTIVE     (0x00U)
#define FlexCAN_MB_CODE_RX_EMPTY        (0x04U)
#define FlexCAN_MB_CODE_RX_FULL         (0x02U)
#define FlexCAN_MB_CODE_RX_OVERRUN      (0x06U)
#define FlexCAN_MB_CODE_RX_BUSY         (0x01U)
#define FlexCAN_MB_CODE_TX_INACTIVE     (0x08U)
#define FlexCAN_MB_CODE_TX_ABORT        (0x09U)
#define FlexCAN_MB_CODE_TX_DATA_ONCE    (0x0CU)
#define FlexCAN_MB_CODE_TX_RESPONSE     (0x0AU)

/* ============================================================================
 * Analog-to-Digital Converter (ADC)
 * ============================================================================ */
typedef struct {
    __IO uint32 SC1[16];                /* 0x000: Status and Control Register 1 */
    __IO uint32 CFG1;                   /* 0x040: Configuration Register 1 */
    __IO uint32 CFG2;                   /* 0x044: Configuration Register 2 */
    __I  uint32 R[16];                  /* 0x048: Data Result Register */
    __IO uint32 CV[2];                  /* 0x088: Compare Value Register */
    __IO uint32 SC2;                    /* 0x090: Status and Control Register 2 */
    __IO uint32 SC3;                    /* 0x094: Status and Control Register 3 */
    __IO uint32 BASE_OFS;               /* 0x098: Base Offset Register */
    __IO uint32 OFS;                    /* 0x09C: Offset Correction Register */
    __IO uint32 USR_OFS;                /* 0x0A0: User Offset Correction Register */
    __IO uint32 XG[4];                  /* 0x0A4: Gain Registers */
    __IO uint32 UG;                     /* 0x0B4: User Gain Register */
    __IO uint32 CLPS;                   /* 0x0B8: General Calibration Value Register */
    __IO uint32 CLP3;                   /* 0x0BC: Calibration Value Register */
    __IO uint32 CLP2;                   /* 0x0C0: Calibration Value Register */
    __IO uint32 CLP1;                   /* 0x0C4: Calibration Value Register */
    __IO uint32 CLP0;                   /* 0x0C8: Calibration Value Register */
    __IO uint32 CLPD;                   /* 0x0CC: General Calibration Value Register */
    __IO uint32 CLPS_OFS;               /* 0x0D0: General Calibration Offset Value Register */
    __IO uint32 CLP3_OFS;               /* 0x0D4: Calibration Offset Value Register */
    __IO uint32 CLP2_OFS;               /* 0x0D8: Calibration Offset Value Register */
    __IO uint32 CLP1_OFS;               /* 0x0DC: Calibration Offset Value Register */
    __IO uint32 CLP0_OFS;               /* 0x0E0: Calibration Offset Value Register */
    __IO uint32 CLPD_OFS;               /* 0x0E4: General Calibration Offset Value Register */
} ADC_Type;

#define ADC0_BASE                       (0x4003B000UL)
#define ADC0                            ((ADC_Type *)ADC0_BASE)

#define ADC_SC1_COCO_MASK               (1UL << 7)
#define ADC_SC1_AIEN_MASK               (1UL << 6)
#define ADC_SC1_ADCH_MASK               (0x1FUL)
#define ADC_SC1_ADCH(x)                 (((uint32)(x)) & ADC_SC1_ADCH_MASK)

/* ============================================================================
 * Low-Power Interrupt Timer (LPIT)
 * ============================================================================ */
typedef struct {
    __IO uint32 MCR;                    /* 0x000: Module Control Register */
    __IO uint32 MSR;                    /* 0x004: Module Status Register */
    __IO uint32 MIER;                   /* 0x008: Module Interrupt Enable Register */
    __IO uint32 SETTEN;                 /* 0x00C: Set Timer Enable Register */
    __IO uint32 CLRTEN;                 /* 0x010: Clear Timer Enable Register */
    uint32 RESERVED_0[2];
    struct {
        __IO uint32 TVAL;               /* 0x020 + 0x10*n: Timer Value Register */
        __I  uint32 CVAL;               /* 0x024 + 0x10*n: Current Timer Value Register */
        __IO uint32 TCTRL;              /* 0x028 + 0x10*n: Timer Control Register */
        uint32 RESERVED;
    } CHANNEL[4];
} LPIT_Type;

#define LPIT0_BASE                      (0x40037000UL)
#define LPIT0                           ((LPIT_Type *)LPIT0_BASE)

#define LPIT_MCR_M_CEN_MASK             (1UL << 0)
#define LPIT_TCTRL_T_EN_MASK            (1UL << 0)

/* ============================================================================
 * Low-Power UART (LPUART) — LPUART1 only, transmit path
 * ============================================================================ */
typedef struct {
    __I  uint32 VERID;                  /* 0x00: Version ID Register */
    __I  uint32 PARAM;                  /* 0x04: Parameter Register */
    __IO uint32 GLOBAL;                 /* 0x08: LPUART Global Register */
    __IO uint32 PINCFG;                 /* 0x0C: LPUART Pin Configuration Register */
    __IO uint32 BAUD;                   /* 0x10: LPUART Baud Rate Register */
    __IO uint32 STAT;                   /* 0x14: LPUART Status Register */
    __IO uint32 CTRL;                   /* 0x18: LPUART Control Register */
    __IO uint32 DATA;                   /* 0x1C: LPUART Data Register */
    __IO uint32 MATCH;                  /* 0x20: LPUART Match Address Register */
    __IO uint32 MODIR;                  /* 0x24: LPUART Modem IrDA Register */
    __IO uint32 FIFO;                   /* 0x28: LPUART FIFO Register */
    __IO uint32 WATER;                  /* 0x2C: LPUART Watermark Register */
} LPUART_Type;

/* LPUART1 is used here because it is the instance routed to the S32K144-EVB's
 * OpenSDA USB-serial debug port on PTC6 (RX, ALT2) / PTC7 (TX, ALT2) — plug
 * the board into USB and the text frames are visible in any serial terminal
 * with no extra wiring. Verify the pin routing against your own board. */
#define LPUART1_BASE                    (0x4006B000UL)
#define LPUART1                         ((LPUART_Type *)LPUART1_BASE)

#define LPUART_BAUD_OSR_SHIFT           (24U)
#define LPUART_BAUD_OSR(x)              (((uint32)((x) - 1U) & 0x1FUL) << LPUART_BAUD_OSR_SHIFT)
#define LPUART_BAUD_SBR_MASK            (0x1FFFUL)
#define LPUART_BAUD_SBR(x)              ((uint32)(x) & LPUART_BAUD_SBR_MASK)
#define LPUART_STAT_TDRE_MASK           (1UL << 23)
#define LPUART_STAT_TC_MASK             (1UL << 22)
#define LPUART_CTRL_TE_MASK             (1UL << 19)

#endif /* S32K144_H */
