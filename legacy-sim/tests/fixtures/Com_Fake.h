/**
 * @file Com_Fake.h
 * @brief Test double for Com (bsw/communication/Com).
 *
 * Serves two distinct fake roles, like tests/fixtures/CanIf_Fake.c does:
 * for `test_pdur` it substitutes the upward notification entry points
 * (`Com_RxIndication`, `Com_TxConfirmation`); for `test_rte_sensor` and
 * `test_rte_cluster` it substitutes the signal API and the reception
 * counters the RTE reads (`Com_SendSignal`, `Com_ReceiveSignal`,
 * `Com_GetIpduRx*`). Which role applies is decided by which real module a
 * test links it against.
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

/** Resets the signal-API role's state (see the two roles above). */
void ComFake_ResetSignals(void);

uint32 ComFake_GetRxIndicationCount(void);
boolean ComFake_GetLastRxIndicationId(PduIdType *out);
/** Copies up to bufferSize bytes of the last Com_RxIndication call's payload; returns FALSE if none
 * yet. */
boolean ComFake_GetLastRxIndicationData(uint8 *buffer, uint8 bufferSize, uint8 *lengthOut);

uint32 ComFake_GetTxConfirmationCount(void);
boolean ComFake_GetLastTxConfirmationId(PduIdType *out);

/* ---- signal-API role, used by the RTE tests ---- */

/** Number of Com_SendSignal calls since the last reset. */
uint32 ComFake_GetSendSignalCount(void);
/** Last value handed to Com_SendSignal for `signal_id`; FALSE if never sent. */
boolean ComFake_GetLastSentSignal(Com_SignalIdType signal_id, uint16 *out);
/** Makes every subsequent Com_SendSignal return this value. */
void ComFake_SetSendSignalResult(uint8 result);

/** Stages the value a later Com_ReceiveSignal for `signal_id` returns. */
void ComFake_SetReceivedSignal(Com_SignalIdType signal_id, uint16 value);
/** Makes every subsequent Com_ReceiveSignal return this value. */
void ComFake_SetReceiveSignalResult(uint8 result);

/** Drives the reception health the RTE derives its status from. */
void ComFake_SetRxTimeout(Com_IPduIdType ipdu_id, boolean timedOut);
void ComFake_SetRxValidCount(Com_IPduIdType ipdu_id, uint32 count);
void ComFake_SetRxCrcErrorCount(Com_IPduIdType ipdu_id, uint32 count);
void ComFake_SetRxCounterErrorCount(Com_IPduIdType ipdu_id, uint32 count);
void ComFake_SetRxSignalRangeErrorCount(Com_IPduIdType ipdu_id, uint32 count);

#endif /* COM_FAKE_H */
