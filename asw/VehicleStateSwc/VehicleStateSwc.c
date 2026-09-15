/**
 * @file VehicleStateSwc.c
 * @brief SensorEcu application component (section 4.1.1).
 *
 * Includes exactly two headers: its own and its generated RTE header
 * (`REQ-ARCH-001`, `REQ-ARCH-002`). It names no COM signal, no PDU, no CAN
 * ID, no IoHwAb function and no MCAL channel — a static check proves it
 * (`REQ-ARCH-008`).
 */
#include "VehicleStateSwc.h"

#include "Rte_VehicleStateSwc.h"

static ClusterStateType g_lastRemoteClusterState;
static Rte_ReturnType g_lastRemoteStatus;
static uint32 g_invalidInputCount;

void VehicleStateSwc_Init(void) {
    /* Local state only — no port write, no transmit (section 4.1.1). */
    g_lastRemoteClusterState = CLUSTER_STATE_COMM_FAULT;
    g_lastRemoteStatus = RTE_E_NEVER_RECEIVED;
    g_invalidInputCount = 0U;
}

void VehicleStateSwc_10ms(void) {
    VehicleSpeedRawType speedRaw = 0U;
    IgnitionOnType ignitionOn = FALSE;
    Rte_InputValidityType validity = RTE_INPUT_INVALID;

    Rte_ReturnType status =
        Rte_Call_RpVehicleInputs_GetVehicleInputs(&speedRaw, &ignitionOn, &validity);

    if ((status != RTE_E_OK) || (validity != RTE_INPUT_VALID)) {
        /* REQ-IOHWAB-003: publish speed 0 and ignition off. Deliberately
         * NOT the last good value — a frozen speed is the more dangerous
         * failure on a cluster. */
        speedRaw = 0U;
        ignitionOn = FALSE;
        g_invalidInputCount++;
    } else if (speedRaw > VEHICLE_SPEED_RAW_MAX) {
        speedRaw = VEHICLE_SPEED_RAW_MAX;
    } else {
        /* value already in range */
    }

    (void)Rte_Write_PpVehicleState_VehicleSpeedRaw(speedRaw);
    (void)Rte_Write_PpVehicleState_IgnitionOn(ignitionOn);
}

void VehicleStateSwc_100ms(void) {
    ClusterStateType remoteState = CLUSTER_STATE_COMM_FAULT;
    Rte_ReturnType status = Rte_Read_RpClusterStatus_ClusterState(&remoteState);

    g_lastRemoteStatus = status;
    /* Only a good read updates the summary; a timeout or an integrity
     * failure leaves the previous state visible next to the status that
     * explains why it is not being refreshed. */
    if (status == RTE_E_OK) {
        g_lastRemoteClusterState = remoteState;
    }
}

ClusterStateType VehicleStateSwc_GetLastRemoteClusterState(void) {
    return g_lastRemoteClusterState;
}

Rte_ReturnType VehicleStateSwc_GetLastRemoteStatus(void) {
    return g_lastRemoteStatus;
}

uint32 VehicleStateSwc_GetInvalidInputCount(void) {
    return g_invalidInputCount;
}
