/**
 * @file ComM.h
 * @brief Communication Manager (ComM).
 *
 * AUTOSAR Classic Architecture:
 * ComM is the resource manager for communication channels. It arbitrates communication
 * requests from users (e.g. BswM, SWCs) and controls Com IPDUs and Bus State Managers (CanSM).
 */
#ifndef COMM_H
#define COMM_H

#include "ComStack_Types.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint8 ComM_ModeType;
#define COMM_NO_COMMUNICATION     ((ComM_ModeType)0U)
#define COMM_SILENT_COMMUNICATION ((ComM_ModeType)1U)
#define COMM_FULL_COMMUNICATION   ((ComM_ModeType)2U)

typedef uint8 ComM_UserHandleType;
#define COMM_USER_BSWM ((ComM_UserHandleType)0U)
#define COMM_USER_APP  ((ComM_UserHandleType)1U)

#define COMM_CHANNEL_CAN ((NetworkHandleType)0U)

typedef uint8 ComM_InitStatusType;
#define COMM_UNINIT ((ComM_InitStatusType)0U)
#define COMM_INIT   ((ComM_InitStatusType)1U)

#define COMM_MAX_CHANNELS ((uint8)2U)
#define COMM_MAX_USERS    ((uint8)4U)

typedef struct {
    NetworkHandleType channelId;
    uint8 numUsers;
    const ComM_UserHandleType *users;
} ComM_ChannelConfigType;

typedef struct {
    const ComM_ChannelConfigType *channels;
    uint8 numChannels;
    uint8 numUsers;
} ComM_ConfigType;

#define COMM_API_ID_INIT                    ((uint8)0x00U)
#define COMM_API_ID_DEINIT                  ((uint8)0x01U)
#define COMM_API_ID_REQUEST_COMM_MODE       ((uint8)0x02U)
#define COMM_API_ID_GET_MAX_COMM_MODE       ((uint8)0x03U)
#define COMM_API_ID_GET_REQUESTED_COMM_MODE ((uint8)0x04U)
#define COMM_API_ID_GET_CURRENT_COMM_MODE   ((uint8)0x05U)
#define COMM_API_ID_MAIN_FUNCTION           ((uint8)0x06U)

Std_ReturnType ComM_Init(const ComM_ConfigType *config);
void ComM_DeInit(void);
Std_ReturnType ComM_RequestComMode(ComM_UserHandleType user, ComM_ModeType mode);
Std_ReturnType ComM_GetMaxComMode(ComM_UserHandleType user, ComM_ModeType *mode);
Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType user, ComM_ModeType *mode);
Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType user, ComM_ModeType *mode);
ComM_ModeType ComM_GetChannelMode(NetworkHandleType channel);
void ComM_MainFunction(void);

#endif /* COMM_H */
