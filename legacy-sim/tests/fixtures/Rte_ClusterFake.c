#include "Rte_ClusterFake.h"

#include <stddef.h>

static VehicleSpeedRawType g_speed;
static IgnitionOnType g_ignition;
static Rte_ReturnType g_readStatus = RTE_E_NEVER_RECEIVED;

static ClusterStateType g_writtenState;
static DisplayedSpeedRawType g_writtenSpeed;
static ClusterStatusFlagsType g_writtenFlags;
static uint32 g_writeCount;

static uint32 g_displayCallCount;
static ClusterStateType g_displayState;
static DisplayedSpeedRawType g_displaySpeed;
static Rte_ReturnType g_displayResult = RTE_E_OK;

Rte_ReturnType Rte_Read_RpVehicleState_VehicleSpeedRaw(VehicleSpeedRawType *value) {
    if (value == NULL) {
        return RTE_E_NOT_OK;
    }
    *value = g_speed;
    return g_readStatus;
}

Rte_ReturnType Rte_Read_RpVehicleState_IgnitionOn(IgnitionOnType *value) {
    if (value == NULL) {
        return RTE_E_NOT_OK;
    }
    *value = g_ignition;
    return g_readStatus;
}

Rte_ReturnType Rte_Write_PpClusterStatus_ClusterState(ClusterStateType value) {
    g_writtenState = value;
    g_writeCount++;
    return RTE_E_OK;
}

Rte_ReturnType Rte_Write_PpClusterStatus_DisplayedSpeedRaw(DisplayedSpeedRawType value) {
    g_writtenSpeed = value;
    g_writeCount++;
    return RTE_E_OK;
}

Rte_ReturnType Rte_Write_PpClusterStatus_StatusFlags(ClusterStatusFlagsType value) {
    g_writtenFlags = value;
    g_writeCount++;
    return RTE_E_OK;
}

Rte_ReturnType Rte_Call_RpDisplay_SetClusterDisplay(ClusterStateType state,
                                                    DisplayedSpeedRawType displayed_speed_raw,
                                                    ClusterStatusFlagsType flags) {
    (void)flags;
    g_displayState = state;
    g_displaySpeed = displayed_speed_raw;
    g_displayCallCount++;
    return g_displayResult;
}

void RteClusterFake_Reset(void) {
    g_speed = 0U;
    g_ignition = FALSE;
    g_readStatus = RTE_E_NEVER_RECEIVED;
    g_writtenState = CLUSTER_STATE_COMM_FAULT;
    g_writtenSpeed = 0U;
    g_writtenFlags = 0U;
    g_writeCount = 0U;
    g_displayCallCount = 0U;
    g_displayState = CLUSTER_STATE_COMM_FAULT;
    g_displaySpeed = 0U;
    g_displayResult = RTE_E_OK;
}

void RteClusterFake_SetVehicleState(VehicleSpeedRawType speed, IgnitionOnType ignition,
                                    Rte_ReturnType status) {
    g_speed = speed;
    g_ignition = ignition;
    g_readStatus = status;
}

ClusterStateType RteClusterFake_GetWrittenState(void) {
    return g_writtenState;
}

DisplayedSpeedRawType RteClusterFake_GetWrittenSpeed(void) {
    return g_writtenSpeed;
}

ClusterStatusFlagsType RteClusterFake_GetWrittenFlags(void) {
    return g_writtenFlags;
}

uint32 RteClusterFake_GetWriteCount(void) {
    return g_writeCount;
}

uint32 RteClusterFake_GetDisplayCallCount(void) {
    return g_displayCallCount;
}

ClusterStateType RteClusterFake_GetDisplayState(void) {
    return g_displayState;
}

DisplayedSpeedRawType RteClusterFake_GetDisplaySpeed(void) {
    return g_displaySpeed;
}

void RteClusterFake_SetDisplayResult(Rte_ReturnType result) {
    g_displayResult = result;
}
