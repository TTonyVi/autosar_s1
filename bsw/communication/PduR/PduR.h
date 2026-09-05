/**
 * @file PduR.h
 * @brief Static PDU Router: id-only routing between COM and CanIf.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 3.1, 7.1, 7.2, 7.4, 7.5,
 * 11.2. `REQ-ARCH-003`: PduR routes by static tables; it SHALL NOT
 * inspect application signal values -- every function here forwards a
 * `PduInfoType` pointer unchanged, never reading past `SduLength`/
 * `SduDataPtr` itself.
 *
 * This project's routing is exactly the two directions of section 7.5:
 * one Tx route (COM -> CanIf) and one Rx route (CanIf -> COM) per ECU.
 * `PduR_CanIfTxConfirmation` looks up the SAME Tx route in reverse (by
 * its CanIf-side id) to recover the COM-side id, since a confirmation
 * flows back along the path the original transmit request took.
 */
#ifndef PDUR_H
#define PDUR_H

#include "ComStack_Types.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef enum { PDUR_MODULE_COM = 0, PDUR_MODULE_CANIF = 1 } PduR_ModuleType;

typedef struct {
    PduR_ModuleType sourceModule;
    PduIdType sourcePduId; /* the id as known to sourceModule */
    PduR_ModuleType destModule;
    PduIdType destPduId; /* the id as known to destModule */
} PduR_RouteType;

typedef struct {
    const PduR_RouteType *routes;
    uint8 numRoutes;
} PduR_ConfigType;

void PduR_Init(const PduR_ConfigType *config);
void PduR_DeInit(void);

/** COM (source) -> CanIf (dest): looks up the Tx route by COM's `id`,
 * then calls `CanIf_Transmit` with the resolved CanIf-side id. */
Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *info);

/** CanIf (source) -> COM (dest): looks up the Rx route by CanIf's `id`,
 * then calls `Com_RxIndication` with the resolved COM-side id. */
void PduR_CanIfRxIndication(PduIdType id, const PduInfoType *info);

/** Reverse lookup on the Tx route by CanIf's `id` (the dest side of that
 * route), then calls `Com_TxConfirmation` with the resolved COM-side id. */
void PduR_CanIfTxConfirmation(PduIdType id);

#endif /* PDUR_H */
