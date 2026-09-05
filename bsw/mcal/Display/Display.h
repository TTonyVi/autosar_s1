/**
 * @file Display.h
 * @brief Display output driver (simplified) — byte-level frame transport
 * only.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 7.4, 11.2, and 3.1's
 * layer table: "Display (MCAL): Byte-level output of one display frame ...
 * Must not know: Why a state is shown, cluster logic." The ASCII frame
 * content (the "STATE=...;SPEED=...;FLAGS=...;T=..." wire format from
 * section 15.5.2) is assembled by IoHwAb (Phase 5); this module only
 * transports whatever bytes it's given.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include "Platform_Types.h"
#include "Std_Types.h"

/** Matches the virtual hardware link's 96-byte cap (section 15.5.2). */
#define DISPLAY_MAX_FRAME_LENGTH ((uint16)96U)

typedef struct {
    uint8 data[DISPLAY_MAX_FRAME_LENGTH];
    uint16 length;
} Display_FrameType;

typedef struct {
    uint8 virtualHwLinkId;
    uint16 maxFrameLength;   /* SHALL be <= DISPLAY_MAX_FRAME_LENGTH */
    uint32 minWritePeriodMs; /* a Display_Write closer than this to the previous one is throttled,
                                not sent */
} Display_ConfigType;

void Display_Init(const Display_ConfigType *config);
void Display_DeInit(void);
Std_ReturnType Display_Write(const Display_FrameType *frame);

/**
 * Phase 2: nothing to poll — Display is transmit-only, no drain needed.
 * Present because section 7.4 requires the API to exist (a real driver
 * might use this to service a hardware output FIFO).
 */
void Display_MainFunction(void);

/* Test/inspection surface. */
uint32 Display_GetWriteSuccessCount(void);
uint32 Display_GetWriteFailureCount(void);
uint32 Display_GetWriteThrottledCount(void);

#endif /* DISPLAY_H */
