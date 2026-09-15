/**
 * @file Rte_VehicleStateSwc.h
 * @brief Generated-style RTE header for VehicleStateSwc (SensorEcu).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.1.1 and 5.3.
 *
 * This is the ONLY RTE header `asw/VehicleStateSwc/` may include
 * (`REQ-ARCH-001`). It exposes port APIs and nothing else: no COM signal
 * ID, no PDU, no CAN ID, no IoHwAb, no MCAL. A SW-C compiled against this
 * header cannot discover where its input came from, which is the whole
 * point of the layer.
 */
#ifndef RTE_VEHICLESTATESWC_H
#define RTE_VEHICLESTATESWC_H

#include "Rte_Type.h"

/* --- PpVehicleState, provided sender port (section 4.1.1) --- */
Rte_ReturnType Rte_Write_PpVehicleState_VehicleSpeedRaw(VehicleSpeedRawType value);
Rte_ReturnType Rte_Write_PpVehicleState_IgnitionOn(IgnitionOnType value);

/* --- RpClusterStatus, required receiver port --- */
Rte_ReturnType Rte_Read_RpClusterStatus_ClusterState(ClusterStateType *value);
Rte_ReturnType Rte_Read_RpClusterStatus_DisplayedSpeedRaw(DisplayedSpeedRawType *value);
Rte_ReturnType Rte_Read_RpClusterStatus_StatusFlags(ClusterStatusFlagsType *value);

/* --- RpVehicleInputs, required client port ---
 * The SW-C asks "what are my inputs and can I trust them"; it never learns
 * that an ADC, a DIO channel, or an abstraction layer was involved. */
Rte_ReturnType Rte_Call_RpVehicleInputs_GetVehicleInputs(VehicleSpeedRawType *speed_raw,
                                                         IgnitionOnType *ignition_on,
                                                         Rte_InputValidityType *validity);

#endif /* RTE_VEHICLESTATESWC_H */
