/**
 * @file ComProtection.h
 * @brief Educational integrity protection for I-PDUs (CRC8 + alive
 * counter), section 6.5.
 *
 * Named `ComProtection`, not `E2E`, because it is not a complete AUTOSAR
 * E2E Profile implementation (section 6.5's own wording). Pure, stateless
 * functions with no sockets, no Det dependency, and no per-I-PDU state of
 * their own -- `Com.c` owns the per-I-PDU alive-counter state (last
 * accepted/next-to-send value) and calls these as building blocks, the
 * same "pure logic split out of the stateful module" pattern already used
 * for `VirtualHw_ParseUint` (Phase 2).
 */
#ifndef COMPROTECTION_H
#define COMPROTECTION_H

#include "Platform_Types.h"

/** Section 6.5: every I-PDU in this project is 8 bytes (section 6.1). */
#define COMPROTECTION_PAYLOAD_LENGTH ((uint8)8U)

/**
 * Computes the CRC8 over {ProtectionDataId high byte, low byte, then all
 * 8 payload bytes with the byte at crcByteIndex temporarily treated as
 * zero} -- section 6.5's exact input sequence and golden vectors.
 * Algorithm: CRC-8/AUTOSAR (polynomial 0x1D, init 0xFF, no reflection,
 * final XOR 0xFF).
 *
 * `crcByteIndex` SHALL be < COMPROTECTION_PAYLOAD_LENGTH; an out-of-range
 * index is treated as "nothing to zero" (the caller -- Com.c, after its
 * own config validation -- never passes one), so this function has no
 * error return and no Det dependency, keeping it pure and trivially
 * testable against the spec's literal golden vectors.
 */
uint8 ComProtection_ComputeCrc8(uint16 protectionDataId,
                                const uint8 payload[COMPROTECTION_PAYLOAD_LENGTH],
                                uint8 crcByteIndex);

/**
 * Section 6.5's alive counter acceptance rule: the first-ever received
 * counter (hasLast == FALSE) always initializes receiver state; every
 * subsequent one SHALL equal (lastCounter + 1) & 0x0F.
 */
boolean ComProtection_IsCounterValid(boolean hasLast, uint8 lastCounter, uint8 receivedCounter);

#endif /* COMPROTECTION_H */
