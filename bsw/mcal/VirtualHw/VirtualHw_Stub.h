/**
 * @file VirtualHw_Stub.h
 * @brief Stub-only instrumentation, additional to VirtualHw.h's contract.
 *
 * A `microcontroller MCAL` port has a real transducer driving its input,
 * so it never needs a function like this one -- it exists only because
 * THIS stub's "in-memory buffer" has no external process feeding it the
 * way a real socket peer (the HMI, a test tool) would. Only test code
 * under tests/ or tools/ may call this (REQ-ARCH-007); no production ECU
 * source does, or could, since normal application code never sees this
 * header (it is not reachable from Adc.h/Dio.h/Display.h).
 */
#ifndef VIRTUALHW_STUB_H
#define VIRTUALHW_STUB_H

#include "VirtualHw.h"

/**
 * RX only. Injects one raw ASCII datagram directly into `linkId`'s
 * in-memory buffer, standing in for a producer process's `sendto()` --
 * the next `VirtualHw_MainFunction_Read` call sees exactly this payload.
 */
void VirtualHw_StubInject(VirtualHw_LinkIdType linkId, const char *payload, uint16 length);

/**
 * TX only. Returns the last payload handed to `VirtualHw_Send` for
 * `linkId`, standing in for "what a peer bound to the real socket would
 * have received". Returns FALSE (buffer untouched) if nothing was ever
 * sent.
 */
boolean VirtualHw_StubGetLastSent(VirtualHw_LinkIdType linkId, char *buffer, uint16 bufferSize);

#endif /* VIRTUALHW_STUB_H */
