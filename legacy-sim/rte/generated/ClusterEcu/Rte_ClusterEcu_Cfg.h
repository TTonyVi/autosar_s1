/**
 * @file Rte_ClusterEcu_Cfg.h
 * @brief ClusterEcu COM signal/I-PDU identifiers: the generated mapping.
 *
 * Mirror of `Rte_SensorEcu_Cfg.h` for the other ECU. Same rationale: this
 * header is the single source of truth shared by the RTE and by the ECU
 * configuration under `ecu/ClusterEcu/`.
 */
#ifndef RTE_CLUSTERECU_CFG_H
#define RTE_CLUSTERECU_CFG_H

/* ClusterEcu transmits ClusterStatus (0x181) and receives VehicleStatus
 * (0x180) — section 7.5. */
#define RTE_IPDU_TX_CLUSTER_STATUS (0U)
#define RTE_IPDU_RX_VEHICLE_STATUS (1U)
#define RTE_NUM_IPDUS (2U)

/* Tx signals of ClusterStatus, section 6.4. */
#define RTE_SIGNAL_TX_CLUSTER_STATE (0U)
#define RTE_SIGNAL_TX_DISPLAYED_SPEED_RAW (1U)
#define RTE_SIGNAL_TX_STATUS_FLAGS (2U)

/* Rx signals of VehicleStatus, section 6.3. */
#define RTE_SIGNAL_RX_VEHICLE_SPEED_RAW (3U)
#define RTE_SIGNAL_RX_IGNITION_ON (4U)

#define RTE_NUM_SIGNALS (5U)

#endif /* RTE_CLUSTERECU_CFG_H */
