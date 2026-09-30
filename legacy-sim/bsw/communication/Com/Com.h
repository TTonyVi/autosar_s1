/**
 * @file Com.h
 * @brief Signal serialization, cyclic I-PDU scheduling, and timeout
 * supervision (simplified AUTOSAR COM).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 6, 7.1, 7.2, 7.4, 7.5,
 * 8.1, 11.2. Packing/unpacking uses shifts and masks only, never a struct
 * cast onto the wire (`REQ-COM-006`); reserved bits/bytes are always
 * transmitted as zero (`REQ-COM-001`).
 *
 * Deviation: real AUTOSAR supports multiple I-PDU groups per ECU; section
 * 9.2 only ever starts "the configured COM I-PDU group" (singular) per
 * ECU in this project, so every configured I-PDU belongs to one implicit
 * group and `Com_IpduGroupStart`/`Com_IpduGroupStop`'s `group_id`
 * parameter (kept for section 7.4's signature) is only checked against
 * `COM_DEFAULT_IPDU_GROUP_ID`.
 *
 * Deviation: since Phase 5's RTE-generated wrappers don't exist yet,
 * `Com_SendSignal`/`Com_ReceiveSignal` always treat `signal_data_ptr` as
 * pointing to a `uint16` -- every signal in section 4.3 fits within 16
 * bits. Real generated RTE code narrows/widens its own native type
 * (`boolean`, a `uint8` enum, ...) to/from `uint16` at the call site.
 *
 * Deviation: `PduR`/`Com` do not exist as a real transport confirmation
 * loop with deferred hardware completion in this project (section 8.1: a
 * host `write()` either queues immediately or fails); "successful
 * transmit request" in section 6.5's alive-counter rule is therefore
 * interpreted as "the call to `PduR_ComTransmit` returned `E_OK`" --
 * the counter does not advance on a request that failed synchronously
 * (e.g. the CAN controller was not started), so it can be retried
 * unchanged next period.
 */
#ifndef COM_H
#define COM_H

#include "ComStack_Types.h"
#include "Gpt.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint16 Com_SignalIdType;
typedef uint16 Com_IPduIdType;
typedef uint8 Com_IpduGroupIdType;

#define COM_DEFAULT_IPDU_GROUP_ID ((Com_IpduGroupIdType)0U)

/** Fixed 8-byte I-PDU length for every I-PDU in this project (section 6.1). */
#define COM_IPDU_LENGTH ((uint8)8U)

typedef enum { COM_IPDU_DIR_TX = 0, COM_IPDU_DIR_RX = 1 } Com_IpduDirectionType;

/**
 * A zero-argument notification, matching real AUTOSAR's
 * `<Signal>_Rx_Notification` shape: it tells the caller (in this phase, a
 * test standing in for the not-yet-existing RTE) that a new, fully valid
 * value is available, without handing the value itself -- the caller
 * reads it back via `Com_ReceiveSignal`, exactly as `Rte_Read` would in
 * Phase 5. May be NULL (no notification wanted for that signal).
 */
typedef void (*Com_SignalNotificationType)(void);

typedef struct {
    Com_SignalIdType
        signalId;          /* == this entry's index in Com_ConfigType.signals, checked at Init */
    Com_IPduIdType ipduId; /* which configured I-PDU owns this signal */
    uint8 bitPosition;     /* starting bit within the 8-byte payload, 0..63, LSB-first per
                            * byte (section 6.1: little-endian for multi-byte values) */
    uint8 bitLength;       /* 1..16 */
    uint16 initValue;
    uint16 minValue;                           /* inclusive */
    uint16 maxValue;                           /* inclusive; REQ-COM-004 checks this on receive */
    Com_SignalNotificationType rxNotification; /* RX-owning-ipdu signals only; NULL otherwise */
} Com_SignalConfigType;

typedef struct {
    Com_IPduIdType ipduId; /* == this entry's index in Com_ConfigType.ipdus, checked at Init */
    Com_IpduDirectionType direction;
    PduIdType pduRPduId;       /* the id this I-PDU is known by on the PduR side (section 7.5) */
    uint8 length;              /* SHALL be COM_IPDU_LENGTH (section 6.1) */
    uint16 periodMs;           /* TX: how often Com_MainFunctionTx sends it (section 6.2) */
    uint16 timeoutMs;          /* RX: deadline monitored by Com_MainFunctionRx (REQ-COM-005) */
    uint8 counterBitPosition;  /* nibble position of the alive counter */
    uint8 counterBitLength;    /* SHALL be 4 (modulo 16, section 6.5) */
    uint8 crcByteIndex;        /* which byte holds the CRC8 */
    uint16 protectionDataId;   /* section 6.5: independent of the physical CAN ID */
    boolean protectionEnabled; /* SHALL be TRUE for both PDUs in this project; kept explicit
                                * and validated rather than assumed */
} Com_IPduConfigType;

typedef struct {
    const Com_IPduConfigType *ipdus;
    uint8 numIpdus;
    const Com_SignalConfigType *signals;
    uint8 numSignals;
} Com_ConfigType;

void Com_Init(const Com_ConfigType *config);
void Com_DeInit(void);

void Com_IpduGroupStart(Com_IpduGroupIdType group_id, boolean initialize);
void Com_IpduGroupStop(Com_IpduGroupIdType group_id);

uint8 Com_SendSignal(Com_SignalIdType signal_id, const void *signal_data_ptr);
uint8 Com_ReceiveSignal(Com_SignalIdType signal_id, void *signal_data_ptr);

/** Cyclic Tx: for each TX I-PDU whose period has elapsed, packs signals +
 * alive counter + CRC8 (reserved bits already zero) and calls
 * `PduR_ComTransmit`. Advances the alive counter only if that call
 * returns `E_OK` (see the file header's deviation note). */
void Com_MainFunctionTx(void);

/** Rx deadline supervision only (unpacking already happened synchronously
 * in `Com_RxIndication`, section 7.2 rule 7): updates each RX I-PDU's
 * timeout status, queryable via `Com_GetIpduRxTimeoutStatus`. */
void Com_MainFunctionRx(void);

/**
 * Called by PduR (section 7.2). Validates DLC before touching the
 * payload (`REQ-COM-002`), then CRC before the alive counter and signals
 * (`REQ-COM-003`), then the alive counter, then every signal's range
 * (`REQ-COM-004`) -- all before committing any state change, so a single
 * invalid frame never partially updates signals (`REQ-ASW-005`). Only a
 * fully valid PDU updates signal values, the alive-counter state, and the
 * last-valid-reception timestamp, and invokes each owned signal's
 * `rxNotification`.
 */
void Com_RxIndication(PduIdType rx_pdu_id, const PduInfoType *pdu_info_ptr);

/** Called by PduR once the underlying transmit has completed (section 7.1
 * rule 9's chain). Counts the confirmation; no further behavior in this
 * phase (Phase 5's RTE will use it for the SW-C's send-confirmation
 * status, not modeled yet). */
void Com_TxConfirmation(PduIdType tx_pdu_id);

/* Test/inspection surface (section 12.1: not an alternate data path). */
uint32 Com_GetIpduTxCount(Com_IPduIdType ipdu_id);
uint32 Com_GetIpduTxFailureCount(Com_IPduIdType ipdu_id);
uint32 Com_GetTxConfirmationCount(Com_IPduIdType ipdu_id);
uint32 Com_GetIpduRxValidCount(Com_IPduIdType ipdu_id);
uint32 Com_GetIpduRxDlcErrorCount(Com_IPduIdType ipdu_id);
uint32 Com_GetIpduRxCrcErrorCount(Com_IPduIdType ipdu_id);
uint32 Com_GetIpduRxCounterErrorCount(Com_IPduIdType ipdu_id);
uint32 Com_GetIpduRxSignalRangeErrorCount(Com_IPduIdType ipdu_id);
/** Returns FALSE (and leaves *isTimedOut untouched) if ipdu_id is invalid
 * or is not an RX I-PDU that has ever received a valid frame. */
boolean Com_GetIpduRxTimeoutStatus(Com_IPduIdType ipdu_id, boolean *isTimedOut);

#endif /* COM_H */
