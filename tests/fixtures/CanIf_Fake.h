/**
 * @file CanIf_Fake.h
 * @brief Test double for CanIf (bsw/communication/CanIf), used only by
 * test_can so the Can Driver can be tested in isolation from CanIf's own
 * routing logic (which test_canif exercises against a fake Can instead).
 *
 * Implements only the two notification entry points Can.c actually calls
 * -- CanIf_RxIndication and CanIf_TxConfirmation, matching CanIf.h's
 * signatures exactly -- backed by simple last-call recording. It does not
 * implement CanIf_Init/DeInit/SetControllerMode/Transmit or the real
 * CanIf's own Get* counters, since nothing in Can.c calls those (same
 * "implement only what the module under test actually calls" convention
 * as tests/fixtures/Gpt_Fake.c).
 *
 * Per spec section 12.1, this file lives under tests/ and is never linked
 * into a production ECU target.
 */
#ifndef CANIF_FAKE_H
#define CANIF_FAKE_H

#include "CanIf.h" /* the two callback signatures this fake substitutes for testing */

/** Clears all recorded calls. */
void CanIfFake_Reset(void);

uint32 CanIfFake_GetRxIndicationCount(void);
/** Returns FALSE if CanIf_RxIndication was never called since the last reset. */
boolean CanIfFake_GetLastRxMailbox(Can_HwType *out);
/** Copies up to bufferSize bytes of the last CanIf_RxIndication's payload; returns FALSE if none
 * yet. */
boolean CanIfFake_GetLastRxData(uint8 *buffer, uint8 bufferSize, uint8 *lengthOut);

uint32 CanIfFake_GetTxConfirmationCount(void);
/** Returns FALSE if CanIf_TxConfirmation was never called since the last reset. */
boolean CanIfFake_GetLastTxConfirmedPduId(PduIdType *out);

#endif /* CANIF_FAKE_H */
