/**
 * @file CanIf_Fake.h
 * @brief Test double for CanIf (bsw/communication/CanIf).
 *
 * Serves two different test roles for the same real module, depending on
 * which direction is under test:
 * - test_can (Phase 3) links this to fake the module ABOVE Can.c: it
 *   implements the two notification entry points Can.c calls upward
 *   (CanIf_RxIndication, CanIf_TxConfirmation).
 * - test_pdur (Phase 4) links this to fake the module BELOW PduR.c: it
 *   implements CanIf_Transmit, the one entry point PduR_ComTransmit calls
 *   downward.
 *
 * It does not implement CanIf_Init/DeInit/SetControllerMode or the real
 * CanIf's own Get* counters, since nothing under test calls those on the
 * fake (same "implement only what's called" convention as
 * tests/fixtures/Gpt_Fake.c).
 *
 * Per spec section 12.1, this file lives under tests/ and is never linked
 * into a production ECU target.
 */
#ifndef CANIF_FAKE_H
#define CANIF_FAKE_H

#include "CanIf.h" /* the callback/entry-point signatures this fake substitutes for testing */

/** Clears all recorded calls and resets CanIf_Transmit's result to E_OK. */
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

/** Controls what CanIf_Transmit returns on every subsequent call. */
void CanIfFake_SetTransmitResult(Std_ReturnType result);
uint32 CanIfFake_GetTransmitCount(void);
/** Returns FALSE if CanIf_Transmit was never called since the last reset. */
boolean CanIfFake_GetLastTransmitId(PduIdType *out);
/** Copies up to bufferSize bytes of the last CanIf_Transmit call's payload; returns FALSE if none
 * yet. */
boolean CanIfFake_GetLastTransmitData(uint8 *buffer, uint8 bufferSize, uint8 *lengthOut);

#endif /* CANIF_FAKE_H */
