/**
 * @file Mcu.h
 * @brief Microcontroller driver (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 7.4. On a real target this
 * configures reset sources, RAM sections, and the clock tree/PLL. Ubuntu
 * has none of that; this module exists so the EcuM initialization order
 * (section 9.2) and the porting story (REQ-ARCH-012) are already correct
 * in shape when a real MCAL replaces this file.
 */
#ifndef MCU_H
#define MCU_H

#include "Platform_Types.h"
#include "Std_Types.h"

typedef struct {
    uint8 reserved; /* no real MCU register config exists on Ubuntu; kept
                        non-empty so the config table shape matches what a
                        real target's clock-tree/reset config would be */
} Mcu_ConfigType;

typedef uint8 Mcu_ClockType;
#define MCU_CLOCK_DEFAULT ((Mcu_ClockType)0U)

void Mcu_Init(const Mcu_ConfigType *config);
Std_ReturnType Mcu_InitClock(Mcu_ClockType clock_setting);

#endif /* MCU_H */
