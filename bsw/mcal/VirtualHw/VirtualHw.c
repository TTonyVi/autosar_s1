/**
 * @file VirtualHw.c
 * @brief Ubuntu implementation of VirtualHw: `AF_UNIX`/`SOCK_DGRAM`
 * sockets, non-blocking. Together with `bsw/mcal/Can/`, this is the
 * complete list of files that touch a host socket in this project
 * (REQ-ARCH-009's static check).
 */

/* POSIX.1-2008 for sys/socket.h, sys/un.h, fcntl.h, unistd.h under
 * -std=c11 with GNU extensions disabled (see Gpt.c for the same pattern
 * with clock_gettime). */
#define _POSIX_C_SOURCE 200809L

#include "VirtualHw.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define VIRTUALHW_API_ID_INIT ((Det_ApiIdType)1U)
#define VIRTUALHW_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)2U)
#define VIRTUALHW_API_ID_GET_LATEST ((Det_ApiIdType)3U)
#define VIRTUALHW_API_ID_SEND ((Det_ApiIdType)4U)

#define VIRTUALHW_MAX_LINKS ((uint8)4U)

/* Larger than VIRTUALHW_MAX_PAYLOAD on purpose: it lets a single recv()
 * capture enough of an oversized datagram to reliably detect and count it
 * as oversized, rather than silently truncating it down to a size that
 * would pass the length check. */
#define VIRTUALHW_RECV_BUFFER_SIZE ((uint16)(VIRTUALHW_MAX_PAYLOAD + 32U))

typedef struct {
    boolean configured;
    VirtualHw_LinkConfigType config;
    int fd;

    char latestPayload[VIRTUALHW_MAX_PAYLOAD + 1U];
    uint16 latestPayloadLength;
    Gpt_ValueType latestReceiveTimeMs;
    boolean hasLatest;

    boolean hasSeq;
    uint32 lastSeq;

    uint32 rxValidCount;
    uint32 rxMalformedCount;
    uint32 rxDuplicateCount;
    uint32 txFailureCount;
} VirtualHw_LinkStateType;

static VirtualHw_LinkStateType g_links[VIRTUALHW_MAX_LINKS];

/* Vai trò: kiểm tra 1 datagram nhận được có "hợp lệ" không — toàn ký tự
 * ASCII in được, có ít nhất 1 dấu "=" (đúng định dạng key=value;...). */
static boolean IsWellFormed(const char *data, uint16 length) {
    if ((length == 0U) || (length > VIRTUALHW_MAX_PAYLOAD)) {
        return FALSE;
    }
    boolean hasEquals = FALSE;
    for (uint16 i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)data[i];
        if ((c < 0x20U) || (c > 0x7EU)) {
            return FALSE; /* not printable ASCII */
        }
        if (c == (unsigned char)'=') {
            hasEquals = TRUE;
        }
    }
    return hasEquals;
}

/* Vai trò: dựng struct sockaddr_un từ đường dẫn file socket (vd
 * "/tmp/vhw_sensor.sock"). */
static void BuildSockaddr(const char *path, struct sockaddr_un *outAddr) {
    (void)memset(outAddr, 0, sizeof(*outAddr));
    outAddr->sun_family = AF_UNIX;
    (void)strncpy(outAddr->sun_path, path, sizeof(outAddr->sun_path) - 1U);
}

/* Vai trò: mở 1 socket AF_UNIX/SOCK_DGRAM và set non-blocking (chưa bind,
 * chưa connect — hai việc đó tuỳ hướng RX/TX làm ở nơi gọi). */
static int OpenNonBlockingUnixDgram(void) {
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        (void)close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

/* Vai trò: khởi tạo tất cả virtual hw link theo config — mở socket
 * AF_UNIX cho từng link; link hướng RX thì unlink file cũ (tránh lỗi
 * "address in use" từ lần chạy trước — REQ-ECUM-006) rồi bind; link
 * hướng TX không cần bind. Bất kỳ lỗi nào cũng KHÔNG làm dừng cả init
 * (REQ-ECUM-005 — thiếu HMI/peer là chuyện bình thường, không phải lỗi
 * khởi động nghiêm trọng như Can). */
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
    for (uint8 i = 0U; i < VIRTUALHW_MAX_LINKS; i++) {
        g_links[i].fd = -1;
    }

    for (uint8 i = 0U; i < config->numLinks; i++) {
        VirtualHw_LinkIdType id = config->links[i].linkId;
        if (id >= VIRTUALHW_MAX_LINKS) {
            (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, i, VIRTUALHW_API_ID_INIT,
                                  DET_ERR_INVALID_CONFIG);
            continue;
        }

        g_links[id].configured = TRUE;
        g_links[id].config = config->links[i];

        int fd = OpenNonBlockingUnixDgram();
        if (fd < 0) {
            (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, id, VIRTUALHW_API_ID_INIT,
                                  DET_ERR_INVALID_CONFIG);
            continue; /* REQ-ECUM-005: not fatal; link stays with fd == -1 */
        }

        if (config->links[i].direction == VIRTUALHW_DIR_RX) {
            /* REQ-ECUM-006: a stale socket file from a previous run is
             * handled deterministically here, not by requiring a manual
             * `rm`. ENOENT (nothing to remove) is expected and ignored. */
            (void)unlink(config->links[i].socketPath);

            struct sockaddr_un addr;
            BuildSockaddr(config->links[i].socketPath, &addr);
            if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
                /* REQ-ECUM-005: a missing/unavailable link SHALL NOT be a
                 * startup failure. */
                (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, id, VIRTUALHW_API_ID_INIT,
                                      DET_ERR_INVALID_CONFIG);
                (void)close(fd);
                fd = -1;
            }
        }
        /* TX role needs no bind: an unbound AF_UNIX SOCK_DGRAM socket can
         * still sendto() a peer path. */

        g_links[id].fd = fd;
    }
}

/* Vai trò: đóng hết socket, xoá file socket của các link RX (dọn sạch
 * cho lần chạy sau), reset toàn bộ state. */
void VirtualHw_DeInit(void) {
    for (uint8 i = 0U; i < VIRTUALHW_MAX_LINKS; i++) {
        if (g_links[i].configured) {
            if (g_links[i].fd >= 0) {
                (void)close(g_links[i].fd);
            }
            if (g_links[i].config.direction == VIRTUALHW_DIR_RX) {
                (void)unlink(g_links[i].config.socketPath);
            }
        }
    }
    (void)memset(g_links, 0, sizeof(g_links));
    for (uint8 i = 0U; i < VIRTUALHW_MAX_LINKS; i++) {
        g_links[i].fd = -1;
    }
}

/* Vai trò: hàm lõi phía nhận — đọc dồn (không block) các datagram mới từ
 * 1 link RX, loại datagram sai định dạng (REQ-VHW-003) hoặc trùng/cũ hơn
 * SEQ đã nhận (REQ-VHW-004, chống replay/out-of-order), rồi lưu lại làm
 * "giá trị mới nhất" kèm mốc thời gian nhận — đây là nơi Adc/Dio thật sự
 * lấy dữ liệu qua VirtualHw_GetLatestPayload. */
void VirtualHw_MainFunction_Read(VirtualHw_LinkIdType linkId) {
    if ((linkId >= VIRTUALHW_MAX_LINKS) || (!g_links[linkId].configured) ||
        (g_links[linkId].config.direction != VIRTUALHW_DIR_RX)) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId, VIRTUALHW_API_ID_MAIN_FUNCTION_READ,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    if (g_links[linkId].fd < 0) {
        return; /* link never bound successfully; nothing to drain, not fatal */
    }

    uint8 maxRx = g_links[linkId].config.maxRxPerActivation;
    for (uint8 i = 0U; i < maxRx; i++) {
        char buf[VIRTUALHW_RECV_BUFFER_SIZE];
        ssize_t n = recv(g_links[linkId].fd, buf, sizeof(buf), 0);
        if (n < 0) {
            break; /* EAGAIN/EWOULDBLOCK: nothing more waiting this cycle (REQ-VHW-001) */
        }

        if (!IsWellFormed(buf, (uint16)n)) {
            g_links[linkId].rxMalformedCount++;
            (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId,
                                  VIRTUALHW_API_ID_MAIN_FUNCTION_READ, DET_ERR_INVALID_PARAM);
            continue; /* REQ-VHW-003: discarded, freshness untouched */
        }

        buf[n] = '\0'; /* safe: IsWellFormed already proved n <= VIRTUALHW_MAX_PAYLOAD */

        uint32 seq;
        if (VirtualHw_ParseUint(buf, "SEQ", &seq) == E_OK) {
            if (g_links[linkId].hasSeq) {
                uint32 diff =
                    seq - g_links[linkId].lastSeq; /* wraps naturally in uint32 arithmetic */
                if ((diff == 0U) || (diff > 0x7FFFFFFFU)) {
                    g_links[linkId].rxDuplicateCount++;
                    continue; /* REQ-VHW-004: not newer than the last accepted SEQ */
                }
            }
            g_links[linkId].lastSeq = seq;
            g_links[linkId].hasSeq = TRUE;
        }

        (void)memcpy(g_links[linkId].latestPayload, buf, (size_t)n);
        g_links[linkId].latestPayload[n] = '\0';
        g_links[linkId].latestPayloadLength = (uint16)n;
        g_links[linkId].latestReceiveTimeMs = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
        g_links[linkId].hasLatest = TRUE;
        g_links[linkId].rxValidCount++;
    }
}

/* Vai trò: trả về payload text mới nhất đã nhận được trên 1 link, kèm
 * tuổi (ms) và cờ "còn tươi hay không" (so với stalenessTimeoutMs cấu
 * hình) — đây là API mà Adc/Dio gọi để lấy dữ liệu cảm biến. */
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

/* Vai trò: hàm lõi phía gửi — sendto() 1 payload text ra 1 link TX (vd
 * Display gửi frame hiển thị ra HMI). Không có ai lắng nghe ở đầu kia
 * (ENOENT) hay lỗi khác đều KHÔNG fatal, chỉ đếm lại (REQ-VHW-005). */
Std_ReturnType VirtualHw_Send(VirtualHw_LinkIdType linkId, const char *payload,
                              uint16 payloadLength) {
    if ((linkId >= VIRTUALHW_MAX_LINKS) || (!g_links[linkId].configured) ||
        (g_links[linkId].config.direction != VIRTUALHW_DIR_TX)) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId, VIRTUALHW_API_ID_SEND,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if ((payload == NULL) || (payloadLength == 0U) || (payloadLength > VIRTUALHW_MAX_PAYLOAD)) {
        (void)Det_ReportError(DET_MODULE_ID_VIRTUALHW, linkId, VIRTUALHW_API_ID_SEND,
                              DET_ERR_INVALID_PARAM);
        g_links[linkId].txFailureCount++;
        return E_NOT_OK;
    }
    if (g_links[linkId].fd < 0) {
        g_links[linkId].txFailureCount++;
        return E_NOT_OK; /* socket never created; treated as a send failure, non-fatal (REQ-VHW-005)
                          */
    }

    struct sockaddr_un addr;
    BuildSockaddr(g_links[linkId].config.socketPath, &addr);

    ssize_t n = sendto(g_links[linkId].fd, payload, payloadLength, 0,
                       (const struct sockaddr *)&addr, sizeof(addr));
    if (n < 0) {
        /* ENOENT (no listener bound yet), ECONNREFUSED, EAGAIN, ... are all
         * non-fatal per REQ-VHW-005: counted, never blocking, never fatal. */
        g_links[linkId].txFailureCount++;
        return E_NOT_OK;
    }
    return E_OK;
}

/* Vai trò: getter — số datagram nhận hợp lệ (đã lọt qua IsWellFormed +
 * kiểm tra SEQ). */
uint32 VirtualHw_GetRxValidCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].rxValidCount : 0U;
}

/* Vai trò: getter — số datagram bị loại vì sai định dạng. */
uint32 VirtualHw_GetRxMalformedCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].rxMalformedCount : 0U;
}

/* Vai trò: getter — số datagram bị loại vì SEQ trùng/cũ hơn. */
uint32 VirtualHw_GetRxDuplicateCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].rxDuplicateCount : 0U;
}

/* Vai trò: getter — số lần VirtualHw_Send thất bại. */
uint32 VirtualHw_GetTxFailureCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_MAX_LINKS) ? g_links[linkId].txFailureCount : 0U;
}
