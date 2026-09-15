/**
 * @file Rte_ClusterControlSwc.h
 * @brief Generated-style RTE header for ClusterControlSwc (ClusterEcu).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.2.1 and 5.3.
 *
 * The only RTE header `asw/ClusterControlSwc/` may include. `RpDisplay` is
 * the SW-C's one route to the panel; it does not know the panel is a
 * Tkinter canvas on the other end of a Unix datagram socket.
 */
#ifndef RTE_CLUSTERCONTROLSWC_H
#define RTE_CLUSTERCONTROLSWC_H

#include "Rte_Type.h"

/* --- RpVehicleState, required receiver port --- */
Rte_ReturnType Rte_Read_RpVehicleState_VehicleSpeedRaw(VehicleSpeedRawType *value);
Rte_ReturnType Rte_Read_RpVehicleState_IgnitionOn(IgnitionOnType *value);

/* --- PpClusterStatus, provided sender port --- */
Rte_ReturnType Rte_Write_PpClusterStatus_ClusterState(ClusterStateType value);
Rte_ReturnType Rte_Write_PpClusterStatus_DisplayedSpeedRaw(DisplayedSpeedRawType value);
Rte_ReturnType Rte_Write_PpClusterStatus_StatusFlags(ClusterStatusFlagsType value);

/* --- RpDisplay, required client port --- */
Rte_ReturnType Rte_Call_RpDisplay_SetClusterDisplay(ClusterStateType state,
                                                    DisplayedSpeedRawType displayed_speed_raw,
                                                    ClusterStatusFlagsType flags);

#endif /* RTE_CLUSTERCONTROLSWC_H */
