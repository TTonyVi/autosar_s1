/**
 * @file Rte_SensorEcu_Cfg.h
 * @brief SensorEcu COM signal/I-PDU identifiers: the generated mapping.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 6.2, 6.3, 6.4 and
 * REQ-RTE-004 ("`Rte_Write` SHALL update the RTE value and then call
 * `Com_SendSignal` using a generated mapping").
 *
 * This header is the single source of truth for those IDs: the RTE uses
 * them to talk to COM, and the ECU configuration under `ecu/SensorEcu/`
 * uses them to build the COM signal table. Defining them twice is exactly
 * the drift this header exists to prevent.
 */
#ifndef RTE_SENSORECU_CFG_H
#define RTE_SENSORECU_CFG_H

/* I-PDUs. SensorEcu transmits VehicleStatus (0x180) and receives
 * ClusterStatus (0x181) — section 7.5. */
#define RTE_IPDU_TX_VEHICLE_STATUS (0U)
#define RTE_IPDU_RX_CLUSTER_STATUS (1U)
#define RTE_NUM_IPDUS (2U)

/* Tx signals of VehicleStatus, section 6.3. */
#define RTE_SIGNAL_TX_VEHICLE_SPEED_RAW (0U)
#define RTE_SIGNAL_TX_IGNITION_ON (1U)

/* Rx signals of ClusterStatus, section 6.4. */
#define RTE_SIGNAL_RX_CLUSTER_STATE (2U)
#define RTE_SIGNAL_RX_DISPLAYED_SPEED_RAW (3U)
#define RTE_SIGNAL_RX_STATUS_FLAGS (4U)

#define RTE_NUM_SIGNALS (5U)

#endif /* RTE_SENSORECU_CFG_H */
