/**
 * @file ClusterControlSwc.c
 * @brief ClusterEcu application component: the cluster state machine.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.2.1 and 4.4.
 *
 * Includes exactly two headers: its own and its generated RTE header
 * (`REQ-ARCH-001`, `REQ-ARCH-002`). A display write failure is visible to
 * this component only as a return value it deliberately ignores — it
 * SHALL NOT change the state machine (section 4.2.1).
 */
#include "ClusterControlSwc.h"

#include "Rte_ClusterControlSwc.h"

static ClusterStateType g_state;
static DisplayedSpeedRawType g_displayedSpeedRaw;
static ClusterStatusFlagsType g_flags;
static uint8 g_consecutiveValid;

void ClusterControlSwc_Init(void) {
    g_state = CLUSTER_STATE_COMM_FAULT;
    g_displayedSpeedRaw = 0U;
    g_flags = CLUSTER_FLAG_TIMEOUT;
    g_consecutiveValid = 0U;
}

/** Runs the section 4.4 transitions for one cycle of valid remote data. */
static void RunValidStateMachine(VehicleSpeedRawType speedRaw, IgnitionOnType ignitionOn) {
    switch (g_state) {
        case CLUSTER_STATE_COMM_FAULT:
            /* REQ-ASW-004: three consecutive valid receptions to recover. */
            if (g_consecutiveValid >= CLUSTER_RECOVERY_FRAME_COUNT) {
                if (!ignitionOn) {
                    g_state = CLUSTER_STATE_OFF;
                } else if (speedRaw >= CLUSTER_OVERSPEED_ENTRY_RAW) {
                    g_state = CLUSTER_STATE_OVERSPEED;
                } else {
                    g_state = CLUSTER_STATE_NORMAL;
                }
            }
            break;

        case CLUSTER_STATE_OFF:
            if (ignitionOn) {
                g_state = CLUSTER_STATE_NORMAL;
            }
            break;

        case CLUSTER_STATE_NORMAL:
            if (!ignitionOn) {
                g_state = CLUSTER_STATE_OFF;
            } else if (speedRaw >= CLUSTER_OVERSPEED_ENTRY_RAW) {
                g_state = CLUSTER_STATE_OVERSPEED;
            } else {
                /* stay NORMAL */
            }
            break;

        case CLUSTER_STATE_OVERSPEED:
            if (!ignitionOn) {
                g_state = CLUSTER_STATE_OFF;
            } else if (speedRaw <= CLUSTER_OVERSPEED_EXIT_RAW) {
                /* REQ-ASW-003: hysteresis — exit only below the lower
                 * threshold, never at the entry threshold. */
                g_state = CLUSTER_STATE_NORMAL;
            } else {
                /* stay OVERSPEED */
            }
            break;

        default:
            /* Unreachable with a bounded enum, but an unknown state is
             * recovered deterministically rather than left undefined. */
            g_state = CLUSTER_STATE_COMM_FAULT;
            break;
    }
}

void ClusterControlSwc_10ms(void) {
    VehicleSpeedRawType speedRaw = 0U;
    IgnitionOnType ignitionOn = FALSE;

    Rte_ReturnType speedStatus = Rte_Read_RpVehicleState_VehicleSpeedRaw(&speedRaw);
    Rte_ReturnType ignitionStatus = Rte_Read_RpVehicleState_IgnitionOn(&ignitionOn);

    boolean commValid = ((speedStatus == RTE_E_OK) && (ignitionStatus == RTE_E_OK)) ? TRUE : FALSE;

    /* REQ-ASW-001: communication invalidity outranks ignition and speed. */
    if (!commValid) {
        g_state = CLUSTER_STATE_COMM_FAULT;
        g_consecutiveValid = 0U;
        g_displayedSpeedRaw = 0U;

        g_flags = 0U;
        if ((speedStatus == RTE_E_TIMEOUT) || (speedStatus == RTE_E_NEVER_RECEIVED) ||
            (ignitionStatus == RTE_E_TIMEOUT) || (ignitionStatus == RTE_E_NEVER_RECEIVED)) {
            g_flags |= CLUSTER_FLAG_TIMEOUT;
        }
        if ((speedStatus == RTE_E_INVALID) || (ignitionStatus == RTE_E_INVALID)) {
            g_flags |= CLUSTER_FLAG_INTEGRITY;
        }
    } else {
        if (g_consecutiveValid < CLUSTER_RECOVERY_FRAME_COUNT) {
            g_consecutiveValid++;
        }

        RunValidStateMachine(speedRaw, ignitionOn);

        /* REQ-ASW-006: displayed speed is clamped to 250.00 km/h. */
        DisplayedSpeedRawType displayed = (DisplayedSpeedRawType)speedRaw;
        if (displayed > VEHICLE_SPEED_RAW_MAX) {
            displayed = VEHICLE_SPEED_RAW_MAX;
        }
        /* Project-local decision, documented: a cluster that is OFF or has
         * not yet recovered shows 0 rather than a number it cannot stand
         * behind. Only OFF/COMM_FAULT are affected; NORMAL and OVERSPEED
         * always show the received value. */
        if ((g_state == CLUSTER_STATE_OFF) || (g_state == CLUSTER_STATE_COMM_FAULT)) {
            displayed = 0U;
        }
        g_displayedSpeedRaw = displayed;

        g_flags = 0U;
        if (g_state == CLUSTER_STATE_OVERSPEED) {
            g_flags |= CLUSTER_FLAG_OVERSPEED;
        }
        if (g_state == CLUSTER_STATE_COMM_FAULT) {
            g_flags |= CLUSTER_FLAG_TIMEOUT;
        }
    }

    (void)Rte_Write_PpClusterStatus_ClusterState(g_state);
    (void)Rte_Write_PpClusterStatus_DisplayedSpeedRaw(g_displayedSpeedRaw);
    (void)Rte_Write_PpClusterStatus_StatusFlags(g_flags);
}

void ClusterControlSwc_100ms(void) {
    /* The return value is deliberately discarded: a display write failure
     * SHALL NOT change the cluster state machine (section 4.2.1). */
    (void)Rte_Call_RpDisplay_SetClusterDisplay(g_state, g_displayedSpeedRaw, g_flags);
}

ClusterStateType ClusterControlSwc_GetState(void) {
    return g_state;
}

DisplayedSpeedRawType ClusterControlSwc_GetDisplayedSpeedRaw(void) {
    return g_displayedSpeedRaw;
}

ClusterStatusFlagsType ClusterControlSwc_GetStatusFlags(void) {
    return g_flags;
}

uint8 ClusterControlSwc_GetConsecutiveValidCount(void) {
    return g_consecutiveValid;
}
