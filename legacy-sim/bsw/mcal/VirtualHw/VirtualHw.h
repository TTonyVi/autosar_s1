/**
 * @file VirtualHw.h
 * @brief Shared non-blocking datagram transport for the virtual hardware
 * links (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 12 and 15.5. This is the
 * second module (after Gpt) allowed to touch a host-specific API — here,
 * `AF_UNIX`/`SOCK_DGRAM` sockets (REQ-ARCH-009's static check). `Adc`,
 * `Dio`, and `Display` all go through this module instead of opening their
 * own sockets, so the socket boundary has exactly one implementation to
 * port when this project moves to real hardware.
 *
 * A link is either:
 * - RX (consumer role): this ECU binds `socketPath` and receives. Used by
 *   `Adc`/`Dio` on `SensorEcu` for the sensor line.
 * - TX (producer role): this ECU sends to `socketPath`, which some other
 *   process (the HMI) has bound. Used by `Display` on `ClusterEcu` for the
 *   display line.
 *
 * Deviation: `VirtualHw_ParseUint` (the generic "KEY=value" extraction
 * helper) lives in the separate, pure translation unit `VirtualHw_Parse.c`
 * with no socket code at all, specifically so unit tests can link it
 * directly alongside a fake VirtualHw implementation (tests/fixtures)
 * without pulling in real socket code — the same seam pattern Phase 1 used
 * for Gpt.
 */
#ifndef VIRTUALHW_H
#define VIRTUALHW_H

#include "Gpt.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint8 VirtualHw_LinkIdType;

typedef enum { VIRTUALHW_DIR_RX = 0, VIRTUALHW_DIR_TX = 1 } VirtualHw_DirectionType;

/** Section 15.5.2: "at most 96 bytes". */
#define VIRTUALHW_MAX_PAYLOAD ((uint16)96U)

typedef struct {
    VirtualHw_LinkIdType linkId;
    VirtualHw_DirectionType direction;
    const char *socketPath;    /* own bind path for RX; destination path for TX */
    uint8 maxRxPerActivation;  /* RX only; default 8 (REQ-VHW-002) */
    uint32 stalenessTimeoutMs; /* RX only; used only for the isFresh convenience flag */
} VirtualHw_LinkConfigType;

typedef struct {
    const VirtualHw_LinkConfigType *links;
    uint8 numLinks;
} VirtualHw_ConfigType;

void VirtualHw_Init(const VirtualHw_ConfigType *config);
void VirtualHw_DeInit(void);

/**
 * RX only. Drains up to `maxRxPerActivation` datagrams for `linkId`,
 * non-blocking (REQ-VHW-001), keeping only the latest valid one
 * (REQ-VHW-002). A malformed or oversized datagram is counted, reported to
 * Det, and discarded without touching the cached payload (REQ-VHW-003). A
 * parseable "SEQ=" field is used to reject datagrams that are not newer
 * than the last accepted one (REQ-VHW-004).
 */
void VirtualHw_MainFunction_Read(VirtualHw_LinkIdType linkId);

/**
 * RX only. Returns TRUE and fills the outputs if at least one valid
 * datagram has ever been received for `linkId`; returns FALSE (outputs
 * left untouched) otherwise. `*ageMsOut` is milliseconds since that
 * datagram was accepted; `*isFreshOut` is TRUE iff that age is within the
 * link's configured staleness timeout. Any output pointer may be NULL if
 * the caller doesn't need it.
 */
boolean VirtualHw_GetLatestPayload(VirtualHw_LinkIdType linkId, char *buffer, uint16 bufferSize,
                                   Gpt_ValueType *ageMsOut, boolean *isFreshOut);

/**
 * TX only. Sends `payload` (`payloadLength` bytes) to `linkId`'s
 * configured destination. Non-blocking; a failure — including "nobody is
 * listening" — is counted and returned as E_NOT_OK, never fatal, never
 * blocking (REQ-VHW-005).
 */
Std_ReturnType VirtualHw_Send(VirtualHw_LinkIdType linkId, const char *payload,
                              uint16 payloadLength);

/**
 * Extracts the decimal value of `key` from a "KEY=value;KEY2=value2;..."
 * ASCII payload (section 15.5.2's wire format). Returns E_NOT_OK if `key`
 * is absent or its value has no digits. Pure function, no state, no
 * sockets — see the file header.
 */
Std_ReturnType VirtualHw_ParseUint(const char *payload, const char *key, uint32 *out);

/* Test/inspection surface (spec section 12.1: not an alternate data path). */
uint32 VirtualHw_GetRxValidCount(VirtualHw_LinkIdType linkId);
uint32 VirtualHw_GetRxMalformedCount(VirtualHw_LinkIdType linkId);
uint32 VirtualHw_GetRxDuplicateCount(VirtualHw_LinkIdType linkId);
uint32 VirtualHw_GetTxFailureCount(VirtualHw_LinkIdType linkId);

#endif /* VIRTUALHW_H */
