/**
 * @file Gpt.h
 * @brief General Purpose Timer driver (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 7.4 and 8.1: `Gpt` is the
 * ONLY module in this project allowed to read the host clock. Every other
 * module — `Os`, `Dem`, later COM/PduR timeout supervision — derives all
 * timing from `Gpt_GetTimeElapsed`, so porting to a real microcontroller
 * changes exactly one file (`Gpt.c`).
 *
 * Deviation from real AUTOSAR: a real Gpt exposes many channels with
 * one-shot/continuous modes and hardware prescalers. This project needs
 * exactly one free-running millisecond time base, started once at
 * `Gpt_Init` and never stopped, so `GPT_CHANNEL_TIME_BASE` is the only
 * channel defined.
 */
#ifndef GPT_H
#define GPT_H

#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint8 Gpt_ChannelType;
typedef uint32 Gpt_ValueType; /* milliseconds elapsed since Gpt_Init */

#define GPT_CHANNEL_TIME_BASE ((Gpt_ChannelType)0U)

typedef struct {
    Gpt_ChannelType channelId; /* must be GPT_CHANNEL_TIME_BASE in this project */
} Gpt_ChannelConfigType;

typedef struct {
    const Gpt_ChannelConfigType *channels;
    uint8 numChannels;
} Gpt_ConfigType;

void Gpt_Init(const Gpt_ConfigType *config);
void Gpt_DeInit(void);

/**
 * Returns milliseconds elapsed since Gpt_Init on the given channel.
 * Returns 0 and reports a Det error if `channel` is not
 * GPT_CHANNEL_TIME_BASE, or if Gpt has not been initialized.
 */
Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType channel);

#endif /* GPT_H */
