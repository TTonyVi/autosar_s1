/**
 * @file Dio.h
 * @brief DIO driver (simplified) — raw ignition level, no interpretation.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.1.2, 7.4, 11.2. Same
 * pass-through contract as Adc.h (REQ-MCAL-001): returns exactly the level
 * last received on its virtual hardware link, with its age. Deciding what
 * "ignition off" means to the application is IoHwAb's/the SW-C's job, not
 * this module's.
 */
#ifndef DIO_H
#define DIO_H

#include "Gpt.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint8 Dio_ChannelType;
typedef Std_LevelType Dio_LevelType; /* mirrors STD_LOW/STD_HIGH */

typedef struct {
    Dio_ChannelType
        channelId; /* selects the wire key "DIO<channelId>"; this project uses only channel 0 */
    uint8 virtualHwLinkId;
    Dio_LevelType
        defaultLevel; /* used only before the first-ever datagram arrives (section 11.2) */
} Dio_ChannelConfigType;

typedef struct {
    const Dio_ChannelConfigType *channels;
    uint8 numChannels;
} Dio_ConfigType;

void Dio_Init(const Dio_ConfigType *config);
void Dio_DeInit(void);
Dio_LevelType Dio_ReadChannel(Dio_ChannelType channel_id);

/**
 * Project-local name for draining the virtual sensor line (spec section
 * 7.4). Deviation: on real hardware this level would be sampled directly
 * from a GPIO register, not by polling a socket.
 */
void Dio_MainFunction_Read(void);

/**
 * Extension beyond spec section 7.4's minimum API, mirroring
 * Adc_GetGroupAge for the same REQ-MCAL-001 reason. Returns UINT32_MAX if
 * no datagram has ever been received (the channel is still on its
 * configured default level).
 */
Gpt_ValueType Dio_GetChannelAge(Dio_ChannelType channel_id);

#endif /* DIO_H */
