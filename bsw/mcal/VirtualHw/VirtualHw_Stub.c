/**
 * @file VirtualHw_Stub.c
 * @brief Microcontroller-MCAL stand-in for VirtualHw.c (REQ-ARCH-012).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 14.3: "Prove REQ-ARCH-012
 * by building both ECUs against a stub MCAL that replaces the links with
 * in-memory buffers, with no change outside bsw/mcal/."
 *
 * Implements the exact same VirtualHw.h contract as VirtualHw.c -- same
 * malformed/oversized rejection (REQ-VHW-003), same SEQ-based
 * duplicate/reorder rejection (REQ-VHW-004), same non-fatal send failure
 * (REQ-VHW-005) -- so `Adc.c`, `Dio.c`, and `Display.c` (all under
 * bsw/mcal/ already, and all consuming ONLY this header, never a socket
 * directly) need zero changes to link against it. No `socket`,
 * `sys/un.h`, or `clock_gettime` call anywhere in this file.
 *
 * "In-memory buffer" here is a single-slot latest-value cell per link,
 * matching REQ-VHW-002's "the most recent valid datagram wins" rule
 * exactly -- it just replaces "arrived over a socket" with "arrived via
 * VirtualHw_StubInject" (RX) or "delivered to a peer" with "recorded for
 * VirtualHw_StubGetLastSent" (TX). `VirtualHw_Stub.h` is the seam test
 * code uses to drive it; no production file reaches that header.
 *
 * `VirtualHw_ParseUint` is unaffected and unchanged -- it already lives in
 * the separate, socket-free `VirtualHw_Parse.c` (see VirtualHw.h), which
 * this variant reuses as-is.
 */
#include "VirtualHw_Stub.h"
#include "VirtualHw.h"

#include <stddef.h>
#include <string.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define VIRTUALHW_API_ID_INIT ((Det_ApiIdType)1U)
#define VIRTUALHW_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)2U)
#define VIRTUALHW_API_ID_GET_LATEST ((Det_ApiIdType)3U)
#define VIRTUALHW_API_ID_SEND ((Det_ApiIdType)4U)

#define VIRTUALHW_MAX_LINKS ((uint8)4U)
#define VIRTUALHW_INJECT_QUEUE_DEPTH ((uint8)8U)

typedef struct {
    boolean configured;
    VirtualHw_LinkConfigType config;

    /* RX role: a small in-order queue of injected datagrams, drained by
     * MainFunction_Read exactly like the real driver drains its socket. */
    char injectQueue[VIRTUALHW_INJECT_QUEUE_DEPTH][VIRTUALHW_MAX_PAYLOAD + 1U];
    uint16 injectLength[VIRTUALHW_INJECT_QUEUE_DEPTH];
    uint8 injectHead;
    uint8 injectCount;

    char latestPayload[VIRTUALHW_MAX_PAYLOAD + 1U];
    uint16 latestPayloadLength;
    Gpt_ValueType latestReceiveTimeMs;
    boolean hasLatest;

    boolean hasSeq;
    uint32 lastSeq;

    /* TX role: what the last VirtualHw_Send call recorded. */
    char lastSent[VIRTUALHW_MAX_PAYLOAD + 1U];
    uint16 lastSentLength;
    boolean hasSent;

    uint32 rxValidCount;
    uint32 rxMalformedCount;
    uint32 rxDuplicateCount;
    uint32 txFailureCount;
} VirtualHw_LinkStateType;

static VirtualHw_LinkStateType g_links[VIRTUALHW_MAX_LINKS];

/* Vai trò: kiểm tra định dạng datagram hợp lệ — giống hệt IsWellFormed
 * của VirtualHw.c thật (section 15.5.2). */
static boolean IsWellFormed(const char *data, uint16 length) {
    if ((length == 0U) || (length > VIRTUALHW_MAX_PAYLOAD)) {
        return FALSE;
    }
    boolean hasEquals = FALSE;
    for (uint16 i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)data[i];
        if ((c < 0x20U) || (c > 0x7EU)) {
            return FALSE;
        }
        if (c == (unsigned char)'=') {
            hasEquals = TRUE;
        }
    }
    return hasEquals;
}

/* Vai trò: giống VirtualHw_Init thật nhưng không mở socket — không có
 * phần cứng để fail nên mọi link cấu hình hợp lệ đều "sẵn sàng" ngay. */
void VirtualHw_Init(const VirtualHw_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, 0U, VIRTUALHW_API_ID_INIT,
                              DET_ERR_NULL_POINTER);
        return;
    }
    if ((config->links == NULL) || (config->numLinks == 0U) ||
        (config->numLinks > VIRTUALHW_MAX_LINKS)) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, 0U, VIRTUALHW_API_ID_INIT,
                              DET_ERR_INVALID_CONFIG);
        return;
    }

    (void)memset(g_links, 0, sizeof(g_links));

    for (uint8 i = 0U; i < config->numLinks; i++) {
        VirtualHw_LinkIdType id = config->links[i].linkId;
        if (id >= VIRTUALHW_MAX_LINKS) {
            (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, i, VIRTUALHW_API_ID_INIT,
                                  DET_ERR_INVALID_CONFIG);
            continue;
        }
        /* No real hardware/socket to fail opening on this variant -- every
         * syntactically valid link is simply ready. */
        g_links[id].configured = TRUE;
        g_links[id].config = config->links[i];
    }
}

/* Vai trò: reset toàn bộ state (xoá cả hàng đợi inject). */
void VirtualHw_DeInit(void) {
    (void)memset(g_links, 0, sizeof(g_links));
}

/* Vai trò: rút datagram từ hàng đợi inject (thay vì đọc socket) — logic
 * lọc malformed/duplicate giống hệt bản thật, chỉ khác nguồn dữ liệu đầu
 * vào là VirtualHw_StubInject() thay vì 1 socket AF_UNIX. */
void VirtualHw_MainFunction_Read(VirtualHw_LinkIdType linkId) {
    if ((linkId >= VIRTUALHW_MAX_LINKS) || (!g_links[linkId].configured) ||
        (g_links[linkId].config.direction != VIRTUALHW_DIR_RX)) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId, VIRTUALHW_API_ID_MAIN_FUNCTION_READ,
                              DET_ERR_INVALID_PARAM);
        return;
    }

    VirtualHw_LinkStateType *link = &g_links[linkId];
    uint8 drained = 0U;
    while ((link->injectCount > 0U) && (drained < link->config.maxRxPerActivation)) {
        uint8 slot = link->injectHead;
        const char *payload = link->injectQueue[slot];
        uint16 length = link->injectLength[slot];
        link->injectHead = (uint8)((link->injectHead + 1U) % VIRTUALHW_INJECT_QUEUE_DEPTH);
        link->injectCount--;
        drained++;

        if (!IsWellFormed(payload, length)) {
            link->rxMalformedCount++;
            (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId,
                                  VIRTUALHW_API_ID_MAIN_FUNCTION_READ, DET_ERR_INVALID_PARAM);
            continue;
        }

        uint32 seq = 0U;
        if (VirtualHw_ParseUint(payload, "SEQ", &seq) == E_OK) {
            if (link->hasSeq) {
                /* Same wraparound-tolerant comparison as VirtualHw.c. */
                sint32 delta = (sint32)(seq - link->lastSeq);
                if (delta <= 0) {
                    link->rxDuplicateCount++;
                    continue;
                }
            }
            link->hasSeq = TRUE;
            link->lastSeq = seq;
        }

        (void)memcpy(link->latestPayload, payload, length);
        link->latestPayload[length] = '\0';
        link->latestPayloadLength = length;
        link->latestReceiveTimeMs = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
        link->hasLatest = TRUE;
        link->rxValidCount++;
    }
}

/* Vai trò: giống hệt bản thật — trả về payload mới nhất + tuổi + cờ
 * "tươi hay không". */
boolean VirtualHw_GetLatestPayload(VirtualHw_LinkIdType linkId, char *buffer, uint16 bufferSize,
                                   Gpt_ValueType *ageMsOut, boolean *isFreshOut) {
    if ((linkId >= VIRTUALHW_MAX_LINKS) || (!g_links[linkId].configured)) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId, VIRTUALHW_API_ID_GET_LATEST,
                              DET_ERR_INVALID_PARAM);
        return FALSE;
    }
    if (!g_links[linkId].hasLatest) {
        return FALSE;
    }

    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    Gpt_ValueType age = now - g_links[linkId].latestReceiveTimeMs;

    if ((buffer != NULL) && (bufferSize > 0U)) {
        uint16 copyLen = g_links[linkId].latestPayloadLength;
        if (copyLen >= bufferSize) {
            copyLen = (uint16)(bufferSize - 1U);
        }
        (void)memcpy(buffer, g_links[linkId].latestPayload, copyLen);
        buffer[copyLen] = '\0';
    }
    if (ageMsOut != NULL) {
        *ageMsOut = age;
    }
    if (isFreshOut != NULL) {
        *isFreshOut = (age <= g_links[linkId].config.stalenessTimeoutMs) ? TRUE : FALSE;
    }
    return TRUE;
}

/* Vai trò: thay vì sendto() ra socket, chỉ lưu payload vào "lastSent" —
 * test đọc lại qua VirtualHw_StubGetLastSent để kiểm tra Display/HMI gửi
 * đúng nội dung gì. */
Std_ReturnType VirtualHw_Send(VirtualHw_LinkIdType linkId, const char *payload,
                              uint16 payloadLength) {
    if ((linkId >= VIRTUALHW_MAX_LINKS) || (!g_links[linkId].configured) ||
        (g_links[linkId].config.direction != VIRTUALHW_DIR_TX) || (payload == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId, VIRTUALHW_API_ID_SEND,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if (payloadLength > VIRTUALHW_MAX_PAYLOAD) {
        g_links[linkId].txFailureCount++;
        return E_NOT_OK;
    }

    (void)memcpy(g_links[linkId].lastSent, payload, payloadLength);
    g_links[linkId].lastSent[payloadLength] = '\0';
    g_links[linkId].lastSentLength = payloadLength;
    g_links[linkId].hasSent = TRUE;
    return E_OK;
}

/* Vai trò: getter — số datagram nhận hợp lệ (giống bản thật). */
uint32 VirtualHw_GetRxValidCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].rxValidCount : 0U;
}

/* Vai trò: getter — số datagram bị loại vì sai định dạng. */
uint32 VirtualHw_GetRxMalformedCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].rxMalformedCount : 0U;
}

/* Vai trò: getter — số datagram bị loại vì trùng/cũ hơn SEQ. */
uint32 VirtualHw_GetRxDuplicateCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].rxDuplicateCount : 0U;
}

/* Vai trò: getter — số lần VirtualHw_Send thất bại. */
uint32 VirtualHw_GetTxFailureCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].txFailureCount : 0U;
}

/* Vai trò: API riêng của bản stub (test dùng) — "bơm" 1 datagram giả vào
 * hàng đợi inject của 1 link, mô phỏng việc có dữ liệu mới tới trên
 * socket thật. */
void VirtualHw_StubInject(VirtualHw_LinkIdType linkId, const char *payload, uint16 length) {
    if ((linkId >= VIRTUALHW_MAX_LINKS) || (!g_links[linkId].configured) || (payload == NULL)) {
        return;
    }
    VirtualHw_LinkStateType *link = &g_links[linkId];
    if (link->injectCount >= VIRTUALHW_INJECT_QUEUE_DEPTH) {
        return; /* bounded, like every other queue in this project */
    }
    uint16 copyLen =
        (length < (uint16)VIRTUALHW_MAX_PAYLOAD) ? length : (uint16)VIRTUALHW_MAX_PAYLOAD;
    uint8 slot = (uint8)((link->injectHead + link->injectCount) % VIRTUALHW_INJECT_QUEUE_DEPTH);
    (void)memcpy(link->injectQueue[slot], payload, copyLen);
    link->injectLength[slot] = copyLen;
    link->injectCount++;
}

/* Vai trò: API riêng của bản stub (test dùng) — đọc lại payload cuối
 * cùng mà VirtualHw_Send đã "gửi", để test kiểm tra nội dung. */
boolean VirtualHw_StubGetLastSent(VirtualHw_LinkIdType linkId, char *buffer, uint16 bufferSize) {
    if ((linkId >= VIRTUALHW_MAX_LINKS) || (!g_links[linkId].hasSent) || (buffer == NULL) ||
        (bufferSize == 0U)) {
        return FALSE;
    }
    uint16 copyLen = g_links[linkId].lastSentLength;
    if (copyLen >= bufferSize) {
        copyLen = (uint16)(bufferSize - 1U);
    }
    (void)memcpy(buffer, g_links[linkId].lastSent, copyLen);
    buffer[copyLen] = '\0';
    return TRUE;
}
