/**
 * @file Com.c
 * @brief Signal shadow buffers, bit-level pack/unpack (shifts and masks
 * only -- REQ-COM-006), cyclic Tx, Rx validation, and deadline
 * supervision. No socket or CAN-specific code anywhere in this file
 * (section 3.1: COM may call only "PduR transmit and RTE indication
 * adapter").
 */
#include "Com.h"

#include <stddef.h>
#include <string.h>

#include "ComProtection.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "PduR.h"

/* REQ-CFG-002: compile-time assertions for the fixed-width types every
 * bit-position calculation in this file depends on, and for the wire
 * length constant used throughout. This project never casts the wire
 * payload to a C struct (REQ-COM-006), so there is no `sizeof(struct) ==
 * 8` to assert against directly -- these pin down the equivalent facts
 * that actually matter here. */
_Static_assert(sizeof(uint8) == 1U, "uint8 must be exactly 1 byte");
_Static_assert(sizeof(uint16) == 2U, "uint16 must be exactly 2 bytes");
_Static_assert(COM_IPDU_LENGTH == 8U, "section 6.1: every I-PDU in this project is 8 bytes");

#define COM_API_ID_INIT ((Det_ApiIdType)1U)
#define COM_API_ID_IPDU_GROUP_START ((Det_ApiIdType)2U)
#define COM_API_ID_IPDU_GROUP_STOP ((Det_ApiIdType)3U)
#define COM_API_ID_SEND_SIGNAL ((Det_ApiIdType)4U)
#define COM_API_ID_RECEIVE_SIGNAL ((Det_ApiIdType)5U)
#define COM_API_ID_RX_INDICATION ((Det_ApiIdType)6U)
#define COM_API_ID_TX_CONFIRMATION ((Det_ApiIdType)7U)

/* This project configures exactly 2 I-PDUs and 5 signals per ECU (section
 * 6.2, 4.3); sized a little above that, matching the small-fixed-table
 * convention used throughout this project. */
#define COM_MAX_IPDUS ((uint8)4U)
#define COM_MAX_SIGNALS ((uint8)8U)
#define COM_IPDU_LENGTH_BITS ((uint16)(COM_IPDU_LENGTH * 8U))

typedef struct {
    uint8 aliveCounter;              /* TX: next value to send; RX: last accepted value */
    boolean hasCounter;              /* RX only: has any valid frame ever been accepted */
    boolean isTimedOut;              /* RX only */
    Gpt_ValueType nextTxReleaseMs;   /* TX only */
    Gpt_ValueType lastValidRxTimeMs; /* RX only */

    uint32 txCount;
    uint32 txFailureCount;
    uint32 txConfirmationCount;
    uint32 rxValidCount;
    uint32 rxDlcErrorCount;
    uint32 rxCrcErrorCount;
    uint32 rxCounterErrorCount;
    uint32 rxSignalRangeErrorCount;
} Com_IpduRuntimeType;

static boolean g_initialized;
static boolean g_groupEnabled;
static Com_ConfigType g_config; /* shallow copy: arrays point into the caller's link-time const
                                 * tables, which SHALL outlive Com_Init (same convention as
                                 * every other *_ConfigType in this project) */
static Com_IpduRuntimeType g_ipdus[COM_MAX_IPDUS];
static uint16 g_signalValues[COM_MAX_SIGNALS];

/* Vai trò: kiểm tra 2 khoảng bit [aStart,aEnd) và [bStart,bEnd) có chồng
 * lấn nhau không — dùng lúc validate config để phát hiện 2 signal (hoặc
 * signal với vùng counter/CRC) tranh nhau cùng 1 bit. */
static boolean RangesOverlap(uint16 aStart, uint16 aEnd, uint16 bStart, uint16 bEnd) {
    return ((aStart < bEnd) && (bStart < aEnd)) ? TRUE : FALSE;
}

/* Vai trò: ghi 1 giá trị (`value`) vào đúng vị trí bit trong 1 frame
 * 8-byte — đây là nơi 1 signal logic (vd tốc độ) được "nhét" vào payload
 * vật lý, bit theo bit, chỉ dùng shift + mask (REQ-COM-006, không ép
 * kiểu struct lên buffer). */
static void PackBits(uint8 frame[COM_IPDU_LENGTH], uint8 bitPosition, uint8 bitLength,
                     uint16 value) {
    for (uint8 i = 0U; i < bitLength; i++) {
        uint8 bitIndex = (uint8)(bitPosition + i);
        uint8 byteIndex = (uint8)(bitIndex / 8U);
        uint8 bitInByte = (uint8)(bitIndex % 8U);
        uint8 bitMask = (uint8)(1U << bitInByte);
        /* The shifted value is assigned to a named uint16 before the
         * mask, rather than combined into one expression, so the `&`
         * operates on two same-width operands with no promote-then-
         * narrow step for the compiler to warn about. Empirically this
         * form (unlike an inline cast immediately before `&`) stays
         * -Wsign-conversion-clean under -fsanitize=undefined too, which
         * changes GCC 9's warning behavior on the exact same expression
         * (see docs/IMPLEMENTATION_STATUS.md, Phase 8). */
        uint16 shifted = (uint16)(value >> i);
        if ((shifted & (uint16)1U) != 0U) {
            frame[byteIndex] = (uint8)(frame[byteIndex] | bitMask);
        } else {
            frame[byteIndex] = (uint8)(frame[byteIndex] & (uint8)(~bitMask));
        }
    }
}

/* Vai trò: chiều ngược lại của PackBits — đọc ra 1 giá trị từ đúng vị trí
 * bit trong frame 8-byte nhận được. */
static uint16 UnpackBits(const uint8 frame[COM_IPDU_LENGTH], uint8 bitPosition, uint8 bitLength) {
    uint16 value = 0U;
    for (uint8 i = 0U; i < bitLength; i++) {
        uint8 bitIndex = (uint8)(bitPosition + i);
        uint8 byteIndex = (uint8)(bitIndex / 8U);
        uint8 bitInByte = (uint8)(bitIndex % 8U);
        /* See the matching comment in PackBits: a named same-width
         * variable before the mask, not an inline cast, is what stays
         * -Wsign-conversion-clean under -fsanitize=undefined too. */
        uint8 shifted = (uint8)(frame[byteIndex] >> bitInByte);
        if ((shifted & (uint8)1U) != 0U) {
            value = (uint16)(value | (uint16)(1U << i));
        }
    }
    return value;
}

/* Vai trò: nạp bảng cấu hình I-PDU + signal tĩnh, validate RẤT kỹ (vị trí
 * bit không chồng lấn nhau, không chồng vào vùng counter/CRC, giá trị
 * init nằm trong [min,max]...) — all-or-nothing. Sau khi qua hết validate
 * thì khởi tạo giá trị signal về initValue và đặt mốc gửi lần đầu. */
void Com_Init(const Com_ConfigType *config) {
    /* Set unconditionally before any validation, so a re-Init that fails
     * partway through can never leave a PREVIOUS successful init's
     * signal/I-PDU tables silently active. */
    g_initialized = FALSE;

    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if ((config->ipdus == NULL) || (config->numIpdus == 0U) || (config->numIpdus > COM_MAX_IPDUS) ||
        (config->numSignals > COM_MAX_SIGNALS) ||
        ((config->numSignals > 0U) && (config->signals == NULL))) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    /* REQ-CFG-001/003/004, section 7.5: reviewable, validated, const
     * tables; any single bad entry fails the whole init (all-or-nothing,
     * same convention as CanIf_Init/PduR_Init). */
    for (uint8 i = 0U; i < config->numIpdus; i++) {
        const Com_IPduConfigType *ipdu = &config->ipdus[i];
        uint16 counterStart = ipdu->counterBitPosition;
        uint16 counterEnd = (uint16)(counterStart + ipdu->counterBitLength);
        uint16 crcStart = (uint16)((uint16)ipdu->crcByteIndex * 8U);
        uint16 crcEnd = (uint16)(crcStart + 8U);

        if ((ipdu->ipduId != (Com_IPduIdType)i) || (ipdu->length != COM_IPDU_LENGTH) ||
            (ipdu->crcByteIndex >= COM_IPDU_LENGTH) || (ipdu->counterBitLength != 4U) ||
            (counterEnd > COM_IPDU_LENGTH_BITS) || (!ipdu->protectionEnabled) ||
            RangesOverlap(counterStart, counterEnd, crcStart, crcEnd)) {
            (void)Det_ReportError(DET_MODULE_ID_COM, i, COM_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            return;
        }
        if (ipdu->direction == COM_IPDU_DIR_TX) {
            if (ipdu->periodMs == 0U) {
                (void)Det_ReportError(DET_MODULE_ID_COM, i, COM_API_ID_INIT,
                                      DET_ERR_INVALID_CONFIG);
                return;
            }
        } else {
            if (ipdu->timeoutMs == 0U) {
                (void)Det_ReportError(DET_MODULE_ID_COM, i, COM_API_ID_INIT,
                                      DET_ERR_INVALID_CONFIG);
                return;
            }
        }
    }

    for (uint8 i = 0U; i < config->numSignals; i++) {
        const Com_SignalConfigType *sig = &config->signals[i];
        uint16 sigStart = sig->bitPosition;
        uint16 sigEnd = (uint16)(sigStart + sig->bitLength);

        if ((sig->signalId != (Com_SignalIdType)i) || (sig->ipduId >= config->numIpdus) ||
            (sig->bitLength == 0U) || (sig->bitLength > 16U) || (sigEnd > COM_IPDU_LENGTH_BITS) ||
            (sig->minValue > sig->maxValue) || (sig->initValue < sig->minValue) ||
            (sig->initValue > sig->maxValue)) {
            (void)Det_ReportError(DET_MODULE_ID_COM, i, COM_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            return;
        }

        const Com_IPduConfigType *owner = &config->ipdus[sig->ipduId];
        uint16 counterStart = owner->counterBitPosition;
        uint16 counterEnd = (uint16)(counterStart + owner->counterBitLength);
        uint16 crcStart = (uint16)((uint16)owner->crcByteIndex * 8U);
        uint16 crcEnd = (uint16)(crcStart + 8U);
        if (RangesOverlap(sigStart, sigEnd, counterStart, counterEnd) ||
            RangesOverlap(sigStart, sigEnd, crcStart, crcEnd)) {
            /* REQ-CFG-003: a signal extending into the counter/CRC bits. */
            (void)Det_ReportError(DET_MODULE_ID_COM, i, COM_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            return;
        }
        for (uint8 j = 0U; j < i; j++) {
            if (config->signals[j].ipduId != sig->ipduId) {
                continue;
            }
            uint16 otherStart = config->signals[j].bitPosition;
            uint16 otherEnd = (uint16)(otherStart + config->signals[j].bitLength);
            if (RangesOverlap(sigStart, sigEnd, otherStart, otherEnd)) {
                /* REQ-CFG-003: invalid bit overlap between two signals. */
                (void)Det_ReportError(DET_MODULE_ID_COM, i, COM_API_ID_INIT,
                                      DET_ERR_INVALID_CONFIG);
                return;
            }
        }
    }

    (void)memset(g_ipdus, 0, sizeof(g_ipdus));
    (void)memset(g_signalValues, 0, sizeof(g_signalValues));
    g_config = *config;

    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    for (uint8 i = 0U; i < config->numIpdus; i++) {
        g_ipdus[i].nextTxReleaseMs = (Gpt_ValueType)(now + config->ipdus[i].periodMs);
    }
    for (uint8 i = 0U; i < config->numSignals; i++) {
        g_signalValues[config->signals[i].signalId] = config->signals[i].initValue;
    }

    g_groupEnabled = FALSE;
    g_initialized = TRUE;
}

/* Vai trò: xoá sạch config + state, hạ mọi cờ. */
void Com_DeInit(void) {
    (void)memset(&g_config, 0, sizeof(g_config));
    (void)memset(g_ipdus, 0, sizeof(g_ipdus));
    (void)memset(g_signalValues, 0, sizeof(g_signalValues));
    g_groupEnabled = FALSE;
    g_initialized = FALSE;
}

/* Vai trò: "bật" nhóm I-PDU — cho phép Com_MainFunctionTx bắt đầu gửi.
 * Nếu initialize=TRUE thì reset luôn alive counter/timeout/giá trị
 * signal về trạng thái ban đầu (dùng lúc EcuM boot). */
void Com_IpduGroupStart(Com_IpduGroupIdType group_id, boolean initialize) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_IPDU_GROUP_START, DET_ERR_UNINIT);
        return;
    }
    if (group_id != COM_DEFAULT_IPDU_GROUP_ID) {
        (void)Det_ReportError(DET_MODULE_ID_COM, group_id, COM_API_ID_IPDU_GROUP_START,
                              DET_ERR_INVALID_PARAM);
        return;
    }

    g_groupEnabled = TRUE;
    if (initialize) {
        Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
        for (uint8 i = 0U; i < g_config.numIpdus; i++) {
            g_ipdus[i].aliveCounter = 0U;
            g_ipdus[i].hasCounter = FALSE;
            g_ipdus[i].isTimedOut = FALSE;
            g_ipdus[i].lastValidRxTimeMs = 0U;
            g_ipdus[i].nextTxReleaseMs = now; /* immediate first send opportunity */
        }
        for (uint8 i = 0U; i < g_config.numSignals; i++) {
            g_signalValues[g_config.signals[i].signalId] = g_config.signals[i].initValue;
        }
    }
}

/* Vai trò: "tắt" nhóm I-PDU — Com_MainFunctionTx ngừng gửi cho tới khi
 * Start lại. */
void Com_IpduGroupStop(Com_IpduGroupIdType group_id) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_IPDU_GROUP_STOP, DET_ERR_UNINIT);
        return;
    }
    if (group_id != COM_DEFAULT_IPDU_GROUP_ID) {
        (void)Det_ReportError(DET_MODULE_ID_COM, group_id, COM_API_ID_IPDU_GROUP_STOP,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    g_groupEnabled = FALSE;
}

/* Vai trò: RTE gọi khi SW-C muốn gửi 1 signal — validate range [min,max],
 * chỉ CẬP NHẬT giá trị vào bộ nhớ đệm (shadow buffer), KHÔNG gửi ngay lên
 * bus (việc gửi thật là Com_MainFunctionTx theo chu kỳ). */
uint8 Com_SendSignal(Com_SignalIdType signal_id, const void *signal_data_ptr) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_SEND_SIGNAL, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if ((signal_id >= g_config.numSignals) || (signal_data_ptr == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_SEND_SIGNAL, DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    const Com_SignalConfigType *sig = &g_config.signals[signal_id];
    if (g_config.ipdus[sig->ipduId].direction != COM_IPDU_DIR_TX) {
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)signal_id, COM_API_ID_SEND_SIGNAL,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    uint16 value = *(const uint16 *)signal_data_ptr;
    if ((value < sig->minValue) || (value > sig->maxValue)) {
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)signal_id, COM_API_ID_SEND_SIGNAL,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    g_signalValues[signal_id] = value;
    return E_OK;
}

/* Vai trò: RTE gọi khi SW-C muốn đọc 1 signal — trả về giá trị mới nhất
 * trong bộ nhớ đệm (đã được Com_RxIndication cập nhật lần nhận gần nhất,
 * hoặc vẫn là initValue nếu chưa nhận được gì). */
uint8 Com_ReceiveSignal(Com_SignalIdType signal_id, void *signal_data_ptr) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_RECEIVE_SIGNAL, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if ((signal_id >= g_config.numSignals) || (signal_data_ptr == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_RECEIVE_SIGNAL,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    *(uint16 *)signal_data_ptr = g_signalValues[signal_id];
    return E_OK;
}

/* Vai trò: hàm chạy theo chu kỳ (20ms) — với mỗi I-PDU hướng Tx đến hạn
 * gửi (dùng mốc TUYỆT ĐỐI, không trôi dồn — giống cách Os.c làm), đóng
 * gói TẤT CẢ signal thuộc I-PDU đó vào 1 frame 8-byte qua PackBits, ghi
 * alive counter, tính CRC8 rồi mới forward xuống PduR_ComTransmit. Gửi
 * thành công mới tăng alive counter (section 6.5). Đây chính là nơi
 * "payload thật sự được tạo ra". */
void Com_MainFunctionTx(void) {
    if ((!g_initialized) || (!g_groupEnabled)) {
        return;
    }
    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    for (uint8 i = 0U; i < g_config.numIpdus; i++) {
        const Com_IPduConfigType *ipdu = &g_config.ipdus[i];
        if (ipdu->direction != COM_IPDU_DIR_TX) {
            continue;
        }
        if (now < g_ipdus[i].nextTxReleaseMs) {
            continue;
        }
        do {
            g_ipdus[i].nextTxReleaseMs =
                (Gpt_ValueType)(g_ipdus[i].nextTxReleaseMs + ipdu->periodMs);
        } while (g_ipdus[i].nextTxReleaseMs <= now);

        uint8 frame[COM_IPDU_LENGTH];
        (void)memset(frame, 0, sizeof(frame));
        for (uint8 s = 0U; s < g_config.numSignals; s++) {
            if (g_config.signals[s].ipduId != ipdu->ipduId) {
                continue;
            }
            PackBits(frame, g_config.signals[s].bitPosition, g_config.signals[s].bitLength,
                     g_signalValues[g_config.signals[s].signalId]);
        }
        PackBits(frame, ipdu->counterBitPosition, ipdu->counterBitLength, g_ipdus[i].aliveCounter);
        frame[ipdu->crcByteIndex] = 0U; /* section 6.5: zero during CRC computation */
        frame[ipdu->crcByteIndex] =
            ComProtection_ComputeCrc8(ipdu->protectionDataId, frame, ipdu->crcByteIndex);

        PduInfoType pduInfo;
        pduInfo.SduDataPtr = frame;
        pduInfo.SduLength = ipdu->length;

        Std_ReturnType r = PduR_ComTransmit(ipdu->pduRPduId, &pduInfo);
        if (r == E_OK) {
            g_ipdus[i].txCount++;
            /* section 6.5: increments modulo 16 after each successful
             * transmit request -- see the file header's deviation note
             * on what "successful" means in this project. */
            g_ipdus[i].aliveCounter = (uint8)((g_ipdus[i].aliveCounter + 1U) & 0x0FU);
        } else {
            g_ipdus[i].txFailureCount++;
        }
    }
}

/* Vai trò: hàm chạy theo chu kỳ — kiểm tra từng I-PDU hướng Rx đã quá
 * timeout kể từ lần nhận hợp lệ gần nhất chưa (REQ-ASW liên quan
 * COMM_FAULT), cập nhật cờ isTimedOut. Không tự nó "nhận" dữ liệu — việc
 * đó là Com_RxIndication. */
void Com_MainFunctionRx(void) {
    if (!g_initialized) {
        return;
    }
    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    for (uint8 i = 0U; i < g_config.numIpdus; i++) {
        if (g_config.ipdus[i].direction != COM_IPDU_DIR_RX) {
            continue;
        }
        if (!g_ipdus[i].hasCounter) {
            continue; /* never received anything valid yet -- nothing to time out from */
        }
        Gpt_ValueType elapsed = (Gpt_ValueType)(now - g_ipdus[i].lastValidRxTimeMs);
        g_ipdus[i].isTimedOut = (elapsed > g_config.ipdus[i].timeoutMs) ? TRUE : FALSE;
    }
}

/* Vai trò: PduR gọi khi có 1 I-PDU mới tới — đây là nơi payload nhận
 * được THẬT SỰ được diễn giải. Trình tự kiểm tra NGHIÊM NGẶT theo đúng
 * thứ tự spec: (1) DLC đúng, (2) CRC8 đúng (REQ-COM-003 — check trước cả
 * counter), (3) alive counter hợp lệ, (4) mọi signal decode ra đều nằm
 * trong range cho phép — TẤT CẢ decode trước, chỉ "commit" (ghi vào
 * signal buffer thật) nếu mọi signal đều ok, để 1 frame lỗi không bao
 * giờ cập nhật dở dang (REQ-COM-004). Bất kỳ bước nào fail thì loại
 * NGUYÊN frame, tăng đúng counter lỗi tương ứng, dừng ngay — giá trị
 * signal cũ giữ nguyên. */
void Com_RxIndication(PduIdType rx_pdu_id, const PduInfoType *pdu_info_ptr) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_RX_INDICATION, DET_ERR_UNINIT);
        return;
    }
    if (rx_pdu_id >= g_config.numIpdus) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_RX_INDICATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    const Com_IPduConfigType *ipdu = &g_config.ipdus[rx_pdu_id];
    if (ipdu->direction != COM_IPDU_DIR_RX) {
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)rx_pdu_id, COM_API_ID_RX_INDICATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    if ((pdu_info_ptr == NULL) || (pdu_info_ptr->SduDataPtr == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)rx_pdu_id, COM_API_ID_RX_INDICATION,
                              DET_ERR_NULL_POINTER);
        return;
    }

    /* REQ-COM-002: verify DLC before accessing payload. */
    if (pdu_info_ptr->SduLength != ipdu->length) {
        g_ipdus[rx_pdu_id].rxDlcErrorCount++;
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)rx_pdu_id, COM_API_ID_RX_INDICATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }

    uint8 payload[COM_IPDU_LENGTH];
    (void)memcpy(payload, pdu_info_ptr->SduDataPtr, COM_IPDU_LENGTH);

    /* REQ-COM-003: CRC validated before the alive counter and signals. */
    uint8 expectedCrc =
        ComProtection_ComputeCrc8(ipdu->protectionDataId, payload, ipdu->crcByteIndex);
    if (payload[ipdu->crcByteIndex] != expectedCrc) {
        g_ipdus[rx_pdu_id].rxCrcErrorCount++;
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)rx_pdu_id, COM_API_ID_RX_INDICATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }

    uint8 receivedCounter =
        (uint8)UnpackBits(payload, ipdu->counterBitPosition, ipdu->counterBitLength);
    boolean counterOk = ComProtection_IsCounterValid(
        g_ipdus[rx_pdu_id].hasCounter, g_ipdus[rx_pdu_id].aliveCounter, receivedCounter);
    if (!counterOk) {
        g_ipdus[rx_pdu_id].rxCounterErrorCount++;
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)rx_pdu_id, COM_API_ID_RX_INDICATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }

    /* REQ-COM-004 + REQ-ASW-005: decode and range-check every owned
     * signal before committing anything, so a single invalid frame can
     * never partially update signal values. */
    uint16 decoded[COM_MAX_SIGNALS];
    boolean allSignalsOk = TRUE;
    for (uint8 s = 0U; s < g_config.numSignals; s++) {
        if (g_config.signals[s].ipduId != rx_pdu_id) {
            continue;
        }
        uint16 value =
            UnpackBits(payload, g_config.signals[s].bitPosition, g_config.signals[s].bitLength);
        decoded[s] = value;
        if ((value < g_config.signals[s].minValue) || (value > g_config.signals[s].maxValue)) {
            allSignalsOk = FALSE;
        }
    }
    if (!allSignalsOk) {
        g_ipdus[rx_pdu_id].rxSignalRangeErrorCount++;
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)rx_pdu_id, COM_API_ID_RX_INDICATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }

    /* Commit: only reached by a fully valid PDU (section 7.2 rule 6). */
    g_ipdus[rx_pdu_id].hasCounter = TRUE;
    g_ipdus[rx_pdu_id].aliveCounter = receivedCounter;
    g_ipdus[rx_pdu_id].lastValidRxTimeMs = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    g_ipdus[rx_pdu_id].isTimedOut = FALSE;
    g_ipdus[rx_pdu_id].rxValidCount++;
    for (uint8 s = 0U; s < g_config.numSignals; s++) {
        if (g_config.signals[s].ipduId != rx_pdu_id) {
            continue;
        }
        g_signalValues[g_config.signals[s].signalId] = decoded[s];
        if (g_config.signals[s].rxNotification != NULL) {
            g_config.signals[s].rxNotification();
        }
    }
}

/* Vai trò: PduR gọi khi 1 I-PDU Tx đã được xác nhận gửi xuống lớp dưới
 * thành công — chỉ đếm lại, phục vụ quan sát/test. */
void Com_TxConfirmation(PduIdType tx_pdu_id) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_COM, 0U, COM_API_ID_TX_CONFIRMATION, DET_ERR_UNINIT);
        return;
    }
    if ((tx_pdu_id >= g_config.numIpdus) ||
        (g_config.ipdus[tx_pdu_id].direction != COM_IPDU_DIR_TX)) {
        (void)Det_ReportError(DET_MODULE_ID_COM, (uint8)tx_pdu_id, COM_API_ID_TX_CONFIRMATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    g_ipdus[tx_pdu_id].txConfirmationCount++;
}

/* Vai trò: getter — số lần gửi thành công của 1 I-PDU. */
uint32 Com_GetIpduTxCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].txCount : 0U;
}

/* Vai trò: getter — số lần gửi thất bại (PduR/CanIf/Can từ chối). */
uint32 Com_GetIpduTxFailureCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].txFailureCount : 0U;
}

/* Vai trò: getter — số lần nhận xác nhận Tx thành công từ lớp dưới. */
uint32 Com_GetTxConfirmationCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].txConfirmationCount : 0U;
}

/* Vai trò: getter — số I-PDU nhận hợp lệ (qua hết DLC/CRC/counter/range). */
uint32 Com_GetIpduRxValidCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].rxValidCount : 0U;
}

/* Vai trò: getter — số I-PDU bị loại vì sai DLC. */
uint32 Com_GetIpduRxDlcErrorCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].rxDlcErrorCount : 0U;
}

/* Vai trò: getter — số I-PDU bị loại vì CRC8 sai (dữ liệu bị hỏng/giả
 * mạo). */
uint32 Com_GetIpduRxCrcErrorCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].rxCrcErrorCount : 0U;
}

/* Vai trò: getter — số I-PDU bị loại vì alive counter sai thứ tự (mất
 * frame/lặp/replay). */
uint32 Com_GetIpduRxCounterErrorCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].rxCounterErrorCount : 0U;
}

/* Vai trò: getter — số I-PDU bị loại vì có signal decode ra ngoài
 * [min,max]. */
uint32 Com_GetIpduRxSignalRangeErrorCount(Com_IPduIdType ipdu_id) {
    return (ipdu_id < COM_MAX_IPDUS) ? g_ipdus[ipdu_id].rxSignalRangeErrorCount : 0U;
}

/* Vai trò: getter — I-PDU hướng Rx này hiện có đang timeout không (đã
 * quá timeoutMs kể từ lần nhận hợp lệ gần nhất). */
boolean Com_GetIpduRxTimeoutStatus(Com_IPduIdType ipdu_id, boolean *isTimedOut) {
    if ((ipdu_id >= g_config.numIpdus) || (g_config.ipdus[ipdu_id].direction != COM_IPDU_DIR_RX) ||
        (!g_ipdus[ipdu_id].hasCounter) || (isTimedOut == NULL)) {
        return FALSE;
    }
    *isTimedOut = g_ipdus[ipdu_id].isTimedOut;
    return TRUE;
}
