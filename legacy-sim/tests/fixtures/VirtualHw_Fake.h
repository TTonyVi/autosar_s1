/**
 * @file VirtualHw_Fake.h
 * @brief Test double for VirtualHw (bsw/mcal/VirtualHw), used only by
 * unit tests.
 *
 * Implements the same public API as VirtualHw.h (Init, DeInit,
 * MainFunction_Read, GetLatestPayload, Send) backed by in-memory state
 * instead of real sockets, so Adc/Dio/Display tests can control exactly
 * what "the latest payload" is and exactly when a send fails, without
 * flakiness from real socket timing. `VirtualHw_ParseUint` is NOT faked
 * here — it's pure logic with no sockets, so tests link the real
 * `VirtualHw_Parse.c` directly alongside this fake (same seam pattern as
 * Phase 1's Gpt_Fake).
 *
 * Per spec section 12.1, this file lives under tests/ and is never linked
 * into a production ECU target.
 */
#ifndef VIRTUALHW_FAKE_H
#define VIRTUALHW_FAKE_H

#include "VirtualHw.h" /* the API this fake substitutes for testing */

/** Clears all fake link state (including delivered payloads and send results). */
void VirtualHwFake_Reset(void);

/**
 * Injects `payload` as if it had just been received on `linkId`, with age
 * 0ms at the moment of injection (age then grows via whatever Gpt is
 * linked in — typically the fake Gpt in the same test).
 */
void VirtualHwFake_Deliver(VirtualHw_LinkIdType linkId, const char *payload);

/** Simulates "nothing has ever been received" on `linkId`. */
void VirtualHwFake_ClearLatest(VirtualHw_LinkIdType linkId);

/** Controls what VirtualHw_Send returns for `linkId` on every subsequent call. */
void VirtualHwFake_SetSendResult(VirtualHw_LinkIdType linkId, Std_ReturnType result);

/** Number of times VirtualHw_Send was called for `linkId`. */
uint32 VirtualHwFake_GetSendCount(VirtualHw_LinkIdType linkId);

#endif /* VIRTUALHW_FAKE_H */
