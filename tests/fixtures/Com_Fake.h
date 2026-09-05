/**
 * @file Com_Fake.h
 * @brief Test double for Com (bsw/communication/Com), used only by
 * test_pdur so PduR's own routing logic can be tested deterministically
 * without a real signal/I-PDU configuration.
 *
 * Implements only the two entry points `PduR.c` actually calls --
 * `Com_RxIndication` and `Com_TxConfirmation` -- matching Com.h's
 * signatures, backed by last-call recording. Does not implement
 * `Com_Init` or any other Com.h function, since nothing in PduR.c calls
 * them (same "implement only what's called" convention as
 * tests/fixtures/CanIf_Fake.c's original role for test_can).
 *
 * Per spec section 12.1, this file lives under tests/ and is never linked
 * into a production ECU target.
 */
#ifndef COM_FAKE_H
#define COM_FAKE_H

#include "Com.h" /* the two callback signatures this fake substitutes for testing */

void ComFake_Reset(void);

uint32 ComFake_GetRxIndicationCount(void);
boolean ComFake_GetLastRxIndicationId(PduIdType *out);
/** Copies up to bufferSize bytes of the last Com_RxIndication call's payload; returns FALSE if none
 * yet. */
boolean ComFake_GetLastRxIndicationData(uint8 *buffer, uint8 bufferSize, uint8 *lengthOut);

uint32 ComFake_GetTxConfirmationCount(void);
boolean ComFake_GetLastTxConfirmationId(PduIdType *out);

#endif /* COM_FAKE_H */
