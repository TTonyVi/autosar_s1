/**
 * @file Rte_SensorEcu.h
 * @brief SensorEcu RTE lifecycle and task entry points (section 5.3).
 *
 * Included by EcuM and by the OS task table, never by a SW-C.
 */
#ifndef RTE_SENSORECU_H
#define RTE_SENSORECU_H

#include "Platform_Types.h"
#include "Std_Types.h"

Std_ReturnType Rte_Start(void);
Std_ReturnType Rte_Stop(void);

/** Task entry functions; the OS calls these, they call the runnables. */
void Rte_Task_10ms(void);
void Rte_Task_100ms(void);

/** COM receive notifications for the ClusterStatus signals. Registered in
 *  the ECU's COM configuration; not called by application code. */
void Rte_COMCbk_ClusterState(void);
void Rte_COMCbk_DisplayedSpeedRaw(void);
void Rte_COMCbk_StatusFlags(void);

/* --- observability for tests and the status log; no control flow --- */
uint32 Rte_GetRxUpdateCount(void);
uint32 Rte_GetTxWriteCount(void);

#endif /* RTE_SENSORECU_H */
