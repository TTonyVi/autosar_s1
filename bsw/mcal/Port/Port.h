/**
 * @file Port.h
 * @brief Pin/port driver (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 7.4. On a real target this
 * configures GPIO direction, pull, and alternate function registers.
 * Ubuntu has no pins; this module exists so the EcuM initialization order
 * (section 9.2, "then pin configuration") is already correct in shape
 * when a real MCAL replaces this file.
 */
#ifndef PORT_H
#define PORT_H

#include "Platform_Types.h"

typedef struct {
    uint8 reserved; /* no real pins exist on Ubuntu; kept non-empty so the
                        config table shape matches a real target's */
} Port_ConfigType;

void Port_Init(const Port_ConfigType *config);

#endif /* PORT_H */
