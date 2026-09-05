/**
 * @file ComStack_Types.h
 * @brief Project-local, AUTOSAR-styled communication stack base types.
 *
 * Verbatim from AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 7.3.
 * These are shared by Com, PduR, CanIf, and the Can Driver starting in
 * Phase 3/4; they are declared here in Phase 0 because they are pure type
 * definitions with no module behavior attached, and every later BSW module
 * depends on this header existing.
 *
 * Deviation from real AUTOSAR: no dedicated NmPdu/CanTp PDU types, no
 * multi-controller HRH pools; this project uses exactly the fields the
 * specification enumerates.
 */
#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Platform_Types.h"

typedef uint16 PduIdType;
typedef uint16 PduLengthType;

typedef struct {
    uint8 *SduDataPtr;
    PduLengthType SduLength;
} PduInfoType;

typedef uint32 Can_IdType;
typedef uint16 Can_HwHandleType;

typedef struct {
    Can_IdType CanId;
    Can_HwHandleType Hoh;
    uint8 ControllerId;
} Can_HwType;

typedef struct {
    PduIdType swPduHandle;
    uint8 length;
    Can_IdType id;
    uint8 *sdu;
} Can_PduType;

#endif /* COMSTACK_TYPES_H */
