/**
 * @file CanSM.h
 * @brief CAN State Manager (CanSM).
 *
 * AUTOSAR Classic Architecture:
 * CanSM handles state management of the CAN communication network, interacting with
 * ComM (upper layer) and CanIf (lower layer).
 */
#ifndef CANSM_H
#define CANSM_H

#include "ComStack_Types.h"
#include "Platform_Types.h"
#include "Std_Types.h"

/* ComM communication mode definitions */
typedef uint8 ComM_ModeType;
#define COMM_NO_COMMUNICATION     ((ComM_ModeType)0U)
#define COMM_SILENT_COMMUNICATION ((ComM_ModeType)1U)
#define COMM_FULL_COMMUNICATION   ((ComM_ModeType)2U)

typedef uint8 CanSM_StateType;
#define CANSM_UNINIT   ((CanSM_StateType)0U)
#define CANSM_NOCOM    ((CanSM_StateType)1U)
#define CANSM_FULLCOM  ((CanSM_StateType)2U)

typedef struct {
    uint8 canControllerId;
    NetworkHandleType networkHandle;
} CanSM_NetworkConfigType;

typedef struct {
    const CanSM_NetworkConfigType *networks;
    uint8 numNetworks;
} CanSM_ConfigType;

#define CANSM_API_ID_INIT                  ((uint8)0x00U)
#define CANSM_API_ID_DEINIT                ((uint8)0x01U)
#define CANSM_API_ID_REQUEST_COMM_MODE     ((uint8)0x02U)
#define CANSM_API_ID_GET_CURRENT_COMM_MODE ((uint8)0x03U)
#define CANSM_API_ID_MAIN_FUNCTION         ((uint8)0x04U)

#define CANSM_MAX_NETWORKS ((uint8)2U)

void CanSM_Init(const CanSM_ConfigType *config);
void CanSM_DeInit(void);
Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType ComM_Mode);
Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType *ComM_Mode);
CanSM_StateType CanSM_GetState(NetworkHandleType network);
void CanSM_MainFunction(void);

#endif /* CANSM_H */
