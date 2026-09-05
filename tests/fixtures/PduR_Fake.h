/**
 * @file PduR_Fake.h
 * @brief Test double for PduR (bsw/communication/PduR), used only by
 * tests.
 *
 * Implements the three notification/transmit entry points real callers
 * actually invoke on PduR from either side -- `PduR_ComTransmit` (called
 * by `Com.c`, used by test_com_pack to capture exactly what bytes COM
 * asked to send without needing a real CanIf/Can), and
 * `PduR_CanIfRxIndication`/`PduR_CanIfTxConfirmation` (called by
 * `CanIf.c`, used by test_canif to verify CanIf forwards correctly
 * without needing a real Com). Does not implement `PduR_Init`/`PduR_DeInit`
 * since nothing under test calls those on the fake.
 *
 * Per spec section 12.1, this file lives under tests/ and is never linked
 * into a production ECU target.
 */
#ifndef PDUR_FAKE_H
#define PDUR_FAKE_H

#include "PduR.h" /* the three entry points this fake substitutes for testing */

/** Clears all recorded calls and resets PduR_ComTransmit's result to E_OK. */
void PduRFake_Reset(void);

/** Controls what PduR_ComTransmit returns on every subsequent call. */
void PduRFake_SetComTransmitResult(Std_ReturnType result);
uint32 PduRFake_GetComTransmitCount(void);
/** Copies up to bufferSize bytes of the last PduR_ComTransmit call's payload; returns FALSE if none
 * yet. */
boolean PduRFake_GetLastComTransmit(PduIdType *idOut, uint8 *dataOut, uint8 bufferSize,
                                    uint8 *lengthOut);

uint32 PduRFake_GetCanIfRxIndicationCount(void);
boolean PduRFake_GetLastCanIfRxIndicationId(PduIdType *out);

uint32 PduRFake_GetCanIfTxConfirmationCount(void);
boolean PduRFake_GetLastCanIfTxConfirmationId(PduIdType *out);

#endif /* PDUR_FAKE_H */
