/**
 * @file CanIf.h
 * @brief CAN Interface: logical PDU <-> physical CAN ID mapping and
 * controller-state gate (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 3.1, 6.1, 7.1, 7.2, 7.4,
 * 7.5, 11.2. CanIf owns "CAN PDU abstraction, ID/filter mapping,
 * controller state" and may call only the Can Driver (section 3.1's
 * table) -- it never touches a socket itself (REQ-ARCH-009 confines
 * sockets to bsw/mcal/).
 *
 * Since Phase 4, `CanIf_RxIndication` forwards an accepted frame to
 * `PduR_CanIfRxIndication` (section 7.2) and a successful `CanIf_Transmit`
 * chain's completion (`CanIf_TxConfirmation`) forwards to
 * `PduR_CanIfTxConfirmation` (section 7.1 rule 9) -- both header-only
 * dependencies resolved at final executable link, same seam as `Can.c`'s
 * own dependency on this header. Unit tests substitute a fake `PduR`
 * instead (tests/fixtures/PduR_Fake.c).
 */
#ifndef CANIF_H
#define CANIF_H

#include "Can.h"
#include "ComStack_Types.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef struct {
    PduIdType canIfTxPduId; /* == this entry's index in CanIf_ConfigType.txPdus, checked at Init */
    Can_IdType canId;       /* physical, standard 11-bit (section 6.1) */
    uint8 dlc;              /* section 6.1: every I-PDU in this project uses 8 */
    Can_HwHandleType hth;   /* forwarded verbatim to Can_Write */
    uint8 controllerId;     /* which Can controller owns this Tx PDU */
} CanIf_TxPduConfigType;

typedef struct {
    PduIdType canIfRxPduId; /* == this entry's index in CanIf_ConfigType.rxPdus, checked at Init */
    Can_IdType canId;
    uint8 dlc;
    uint8 controllerId;
} CanIf_RxPduConfigType;

typedef struct {
    const CanIf_TxPduConfigType *txPdus;
    uint8 numTxPdus;
    const CanIf_RxPduConfigType *rxPdus;
    uint8 numRxPdus;
} CanIf_ConfigType;

void CanIf_Init(const CanIf_ConfigType *config);
void CanIf_DeInit(void);

/** Validating pass-through to Can_SetControllerMode (section 3.1: CanIf
 * "may call" only the Can Driver). */
Std_ReturnType CanIf_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode);

/**
 * Section 7.5's static routing: `tx_pdu_id` SHALL be a valid index into
 * the configured Tx table, `info->SduLength` SHALL equal that entry's
 * configured DLC, and the owning controller SHALL be CAN_CS_STARTED --
 * any violation is rejected (Det record, E_NOT_OK) without ever calling
 * Can_Write (Phase 3 exit criteria: wrong ID/DLC/controller-state cases
 * are tested).
 */
Std_ReturnType CanIf_Transmit(PduIdType tx_pdu_id, const PduInfoType *info);

/**
 * Called by the Can Driver (section 7.1 rule 9, section 7.2's sequence
 * diagram). A CAN ID/controller pair with no matching configured Rx PDU,
 * or a matching one with the wrong DLC, is rejected (counted, Det record,
 * dropped) -- see the Phase-3-scope note above for why nothing is
 * forwarded further yet.
 */
void CanIf_RxIndication(const Can_HwType *mailbox, const PduInfoType *info);
void CanIf_TxConfirmation(PduIdType tx_pdu_id);

/* Test/inspection surface (section 12.1: not an alternate data path). */
uint32 CanIf_GetRxAcceptedCount(void);
uint32 CanIf_GetRxRejectedCount(void); /* unmapped CAN ID or wrong DLC */
uint32 CanIf_GetTxConfirmationCount(void);
Std_ReturnType CanIf_GetLastRxPduId(PduIdType *out);

#endif /* CANIF_H */
