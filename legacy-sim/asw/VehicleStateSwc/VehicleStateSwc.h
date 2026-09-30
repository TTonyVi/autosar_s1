/**
 * @file VehicleStateSwc.h
 * @brief Runnable entry points of the SensorEcu application component.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 4.1.1. Declared for the
 * RTE to call; no BSW module ever includes this header.
 */
#ifndef VEHICLESTATESWC_H
#define VEHICLESTATESWC_H

#include "Rte_Type.h"

/** Activated by `Rte_Start`; initializes local state only, never transmits. */
void VehicleStateSwc_Init(void);

/** Timing event, 10 ms: read inputs through the RTE, apply the
 *  invalid-input policy, publish vehicle state. */
void VehicleStateSwc_10ms(void);

/** Timing event, 100 ms: read remote cluster status into a local
 *  supervision summary used for logging only. */
void VehicleStateSwc_100ms(void);

/* --- local supervision summary, read by the status log and by tests --- */
ClusterStateType VehicleStateSwc_GetLastRemoteClusterState(void);
Rte_ReturnType VehicleStateSwc_GetLastRemoteStatus(void);
uint32 VehicleStateSwc_GetInvalidInputCount(void);

#endif /* VEHICLESTATESWC_H */
