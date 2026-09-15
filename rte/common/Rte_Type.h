/**
 * @file Rte_Type.h
 * @brief Application data types shared by SW-Cs, the RTE, and IoHwAb.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.3 and 5.2.
 *
 * These are the ONLY data definitions a SW-C is allowed to see besides its
 * own generated `Rte_<Swc>.h` (`REQ-ARCH-001`). Nothing here names a CAN
 * ID, a PDU, a byte layout, a socket, or an MCAL channel — a SW-C that
 * includes this header still cannot learn where its input came from.
 */
#ifndef RTE_TYPE_H
#define RTE_TYPE_H

#include "Platform_Types.h"
#include "Std_Types.h"

/* ---------------- section 4.3: application data types ---------------- */

/** Vehicle speed in 0.01 km/h per bit, 0..25000 (0.00..250.00 km/h). */
typedef uint16 VehicleSpeedRawType;

/** Ignition state. */
typedef boolean IgnitionOnType;

/** Displayed speed in 0.01 km/h per bit, 0..25000. */
typedef uint16 DisplayedSpeedRawType;

/** Cluster state, 0..3. */
typedef uint8 ClusterStateType;

#define CLUSTER_STATE_OFF ((ClusterStateType)0U)
#define CLUSTER_STATE_NORMAL ((ClusterStateType)1U)
#define CLUSTER_STATE_OVERSPEED ((ClusterStateType)2U)
#define CLUSTER_STATE_COMM_FAULT ((ClusterStateType)3U)

/** Warning and communication status bit field. Bits 3..7 transmit as zero. */
typedef uint8 ClusterStatusFlagsType;

#define CLUSTER_FLAG_OVERSPEED ((ClusterStatusFlagsType)0x01U) /* bit 0 */
#define CLUSTER_FLAG_TIMEOUT ((ClusterStatusFlagsType)0x02U)   /* bit 1 */
#define CLUSTER_FLAG_INTEGRITY ((ClusterStatusFlagsType)0x04U) /* bit 2 */
#define CLUSTER_FLAGS_VALID_MASK ((ClusterStatusFlagsType)0x07U)

/** Highest value either raw speed type may carry (250.00 km/h). */
#define VEHICLE_SPEED_RAW_MAX ((VehicleSpeedRawType)25000U)

/* ---------------- section 4.4: state machine thresholds -------------- */

/** REQ-ASW-002: overspeed entry, 100.00 km/h. */
#define CLUSTER_OVERSPEED_ENTRY_RAW ((VehicleSpeedRawType)10000U)
/** REQ-ASW-003: overspeed exit, 97.00 km/h (hysteresis). */
#define CLUSTER_OVERSPEED_EXIT_RAW ((VehicleSpeedRawType)9700U)
/** REQ-ASW-004: consecutive valid receptions needed to leave COMM_FAULT. */
#define CLUSTER_RECOVERY_FRAME_COUNT ((uint8)3U)

/* ---------------- section 5.2: RTE return values --------------------- */

typedef uint8 Rte_ReturnType;

#define RTE_E_OK ((Rte_ReturnType)0U)
#define RTE_E_NOT_OK ((Rte_ReturnType)1U)
#define RTE_E_NEVER_RECEIVED ((Rte_ReturnType)2U)
#define RTE_E_TIMEOUT ((Rte_ReturnType)3U)
#define RTE_E_INVALID ((Rte_ReturnType)4U)
#define RTE_E_HARDWARE_INVALID ((Rte_ReturnType)5U)

/**
 * Mirrors `IoHwAb_ValidityType` one to one (section 5.2). This is the only
 * form in which hardware health reaches a SW-C: it learns that its input is
 * untrustworthy, never why, and never that a socket was involved.
 */
typedef uint8 Rte_InputValidityType;

#define RTE_INPUT_VALID ((Rte_InputValidityType)0U)
#define RTE_INPUT_INVALID ((Rte_InputValidityType)1U)

#endif /* RTE_TYPE_H */
