/**
 * @file Rte_ClusterFake.h
 * @brief Test double for the ClusterEcu RTE, used by test_cluster_swc.
 *
 * Substitutes the generated port APIs declared in
 * `Rte_ClusterControlSwc.h` so the state machine can be driven through
 * every transition of section 4.4 with exact control over what each read
 * returns — including the statuses (`RTE_E_TIMEOUT`, `RTE_E_INVALID`,
 * `RTE_E_NEVER_RECEIVED`) that a real COM would take many cycles to
 * produce.
 */
#ifndef RTE_CLUSTERFAKE_H
#define RTE_CLUSTERFAKE_H

#include "Rte_ClusterControlSwc.h"

void RteClusterFake_Reset(void);

/** Stages what the next RpVehicleState reads return. */
void RteClusterFake_SetVehicleState(VehicleSpeedRawType speed, IgnitionOnType ignition,
                                    Rte_ReturnType status);

/* --- what the SW-C last wrote to PpClusterStatus --- */
ClusterStateType RteClusterFake_GetWrittenState(void);
DisplayedSpeedRawType RteClusterFake_GetWrittenSpeed(void);
ClusterStatusFlagsType RteClusterFake_GetWrittenFlags(void);
uint32 RteClusterFake_GetWriteCount(void);

/* --- what the SW-C last sent to RpDisplay --- */
uint32 RteClusterFake_GetDisplayCallCount(void);
ClusterStateType RteClusterFake_GetDisplayState(void);
DisplayedSpeedRawType RteClusterFake_GetDisplaySpeed(void);
/** Makes every subsequent display call fail, to prove it cannot move the
 *  state machine (section 4.2.1). */
void RteClusterFake_SetDisplayResult(Rte_ReturnType result);

#endif /* RTE_CLUSTERFAKE_H */
