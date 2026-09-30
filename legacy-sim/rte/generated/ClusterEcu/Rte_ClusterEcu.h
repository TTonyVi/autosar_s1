/**
 * @file Rte_ClusterEcu.h
 * @brief ClusterEcu RTE lifecycle and task entry points (section 5.3).
 */
#ifndef RTE_CLUSTERECU_H
#define RTE_CLUSTERECU_H

#include "Platform_Types.h"
#include "Std_Types.h"

Std_ReturnType Rte_Start(void);
Std_ReturnType Rte_Stop(void);

void Rte_Task_10ms(void);
void Rte_Task_100ms(void);

/** COM receive notifications for the VehicleStatus signals. */
void Rte_COMCbk_VehicleSpeedRaw(void);
void Rte_COMCbk_IgnitionOn(void);

uint32 Rte_GetRxUpdateCount(void);
uint32 Rte_GetTxWriteCount(void);

#endif /* RTE_CLUSTERECU_H */
