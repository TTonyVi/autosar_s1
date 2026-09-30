/**
 * @file BswM.h
 * @brief Basic Software Mode Manager (BSW Mode Manager).
 *
 * Implements the AUTOSAR Classic BswM role: Post-OS initialization and mode arbitration
 * through Action Lists (Driver Init, Com Stack Init, RTE Start, Com Control).
 */
#ifndef BSWM_H
#define BSWM_H

#include "Platform_Types.h"
#include "Std_Types.h"

#include "Adc.h"
#include "Can.h"
#include "CanIf.h"
#include "CanSM.h"
#include "Com.h"
#include "ComM.h"
#include "Dio.h"
#include "Display.h"
#include "IoHwAb.h"
#include "PduR.h"

typedef uint8 BswM_StateType;

#define BSWM_STATE_UNINIT   ((BswM_StateType)0U)
#define BSWM_STATE_STARTUP  ((BswM_StateType)1U)
#define BSWM_STATE_RUN      ((BswM_StateType)2U)
#define BSWM_STATE_SHUTDOWN ((BswM_StateType)3U)

typedef struct {
    const Adc_ConfigType *adcConfig;
    const Dio_ConfigType *dioConfig;
    const Display_ConfigType *displayConfig;
    const Can_ConfigType *canConfig;
    const IoHwAb_ConfigType *ioHwAbConfig;
    const CanIf_ConfigType *canIfConfig;
    const CanSM_ConfigType *canSMConfig;
    const PduR_ConfigType *pduRConfig;
    const Com_ConfigType *comConfig;
    const ComM_ConfigType *comMConfig;
    uint8 canControllerId;
} BswM_ConfigType;

#define BSWM_API_ID_INIT             ((uint8)0x00U)
#define BSWM_API_ID_DEINIT           ((uint8)0x01U)
#define BSWM_API_ID_MAIN_FUNCTION    ((uint8)0x02U)
#define BSWM_API_ID_REQUEST_MODE     ((uint8)0x03U)

#define BSWM_E_UNINIT                ((uint8)0x01U)
#define BSWM_E_NULL_POINTER          ((uint8)0x02U)
#define BSWM_E_ACTION_FAILED         ((uint8)0x03U)

Std_ReturnType BswM_Init(const BswM_ConfigType *config);
void BswM_DeInit(void);
BswM_StateType BswM_GetState(void);
void BswM_MainFunction(void);

#endif /* BSWM_H */
