/**
 * @file Can.c
 * @brief Ubuntu implementation of Can: `PF_CAN`/`SOCK_RAW`/`CAN_RAW`
 * sockets, non-blocking. Together with `bsw/mcal/VirtualHw/`, this is the
 * complete list of files that touch a host socket in this project
 * (REQ-ARCH-009's static check).
 */

/* POSIX.1-2008 for sys/socket.h, fcntl.h, unistd.h under -std=c11 with GNU
 * extensions disabled (see Gpt.c for the same pattern with clock_gettime).
 * _DEFAULT_SOURCE is additionally required for `struct ifreq`
 * (net/if.h): glibc only exposes that BSD-ism when _DEFAULT_SOURCE is
 * explicitly requested, and defining _POSIX_C_SOURCE alone turns off the
 * feature-test-macro default that would otherwise imply it. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "Can.h"

#include <fcntl.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>

/* Older glibc versions do not define these even though the kernel headers
 * above assume them; harmless if already defined by <sys/socket.h>. */
#ifndef PF_CAN
#define PF_CAN 29
#endif
#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

#include "CanIf.h"
#include "Det.h"
#include "Det_ModuleIds.h"

#define CAN_API_ID_INIT ((Det_ApiIdType)1U)
#define CAN_API_ID_SET_CONTROLLER_MODE ((Det_ApiIdType)2U)
#define CAN_API_ID_WRITE ((Det_ApiIdType)3U)
#define CAN_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)4U)

/* Only one controller per ECU is actually used in this project; the table
 * is sized for two, matching the small-fixed-table convention already
 * used for VirtualHw/Adc/Dio/Display links and groups. */
#define CAN_MAX_CONTROLLERS ((uint8)2U)

/** Bounded drain per controller per Can_MainFunction_Read call (section
 * 7.2 rule 1's non-blocking requirement; same sibling rule as
 * VirtualHw's REQ-VHW-002). */
#define CAN_RX_PER_ACTIVATION ((uint8)8U)

typedef struct {
    boolean configured;
    Can_ControllerConfigType config;
    int fd;
    Can_ControllerStateType state;

    uint32 rxFrameCount;
    uint32 rxDroppedCount;
    uint32 txFailureCount;
} Can_ControllerStateEntryType;

static Can_ControllerStateEntryType g_controllers[CAN_MAX_CONTROLLERS];

/* Vai trò: chuyển 1 file descriptor sang chế độ non-blocking (đọc/ghi
 * không chờ), bắt buộc vì Can_MainFunction_Read chạy theo chu kỳ, không
 * được phép block chờ dữ liệu. */
static boolean SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return FALSE;
    }
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0) ? TRUE : FALSE;
}

/* Vai trò: mở 1 socket CAN thô (PF_CAN/SOCK_RAW), tra chỉ số interface
 * theo tên ("vcan0"), bind socket vào đúng interface đó, rồi set
 * non-blocking. Trả về fd hoặc -1 nếu bất kỳ bước nào lỗi. */
static int OpenAndBindCanSocket(const char *interfaceName) {
    int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    (void)memset(&ifr, 0, sizeof(ifr));
    (void)strncpy(ifr.ifr_name, interfaceName, sizeof(ifr.ifr_name) - 1U);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        (void)close(fd);
        return -1;
    }

    struct sockaddr_can addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
        (void)close(fd);
        return -1;
    }

    if (!SetNonBlocking(fd)) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

/* Vai trò: khởi tạo tất cả controller CAN được liệt kê trong config —
 * validate config, mở + bind socket thật cho từng controller. Mở socket
 * lỗi không làm dừng cả hàm (ghi nhận qua Det, controller đó ở lại
 * UNINIT); EcuM (Phase 6) là nơi quyết định lỗi này có chặn boot hay
 * không. */
void Can_Init(const Can_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if ((config->controllers == NULL) || (config->numControllers == 0U) ||
        (config->numControllers > CAN_MAX_CONTROLLERS)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    (void)memset(g_controllers, 0, sizeof(g_controllers));
    for (uint8 i = 0U; i < CAN_MAX_CONTROLLERS; i++) {
        g_controllers[i].fd = -1;
        g_controllers[i].state = CAN_CS_UNINIT;
    }

    for (uint8 i = 0U; i < config->numControllers; i++) {
        uint8 id = config->controllers[i].controllerId;
        if (id >= CAN_MAX_CONTROLLERS) {
            (void)Det_ReportError(DET_MODULE_ID_CAN, id, CAN_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            continue;
        }
        if ((config->controllers[i].interfaceName == NULL) ||
            (!config->controllers[i].nonBlocking)) {
            (void)Det_ReportError(DET_MODULE_ID_CAN, id, CAN_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            continue;
        }

        g_controllers[id].configured = TRUE;
        g_controllers[id].config = config->controllers[i];
        g_controllers[id].state = CAN_CS_STOPPED; /* hardware reachable, not yet started */

        int fd = OpenAndBindCanSocket(config->controllers[i].interfaceName);
        if (fd < 0) {
            /* REQ-ECUM-001: unlike a VirtualHw link (REQ-ECUM-005), this
             * SHALL be a startup failure -- EcuM (Phase 6) checks
             * Can_IsControllerReady and aborts. This driver itself stays
             * non-fatal: it records the failure and moves on. */
            (void)Det_ReportError(DET_MODULE_ID_CAN, id, CAN_API_ID_INIT, DET_ERR_HW_INIT_FAILED);
            g_controllers[id].state = CAN_CS_UNINIT;
            continue;
        }
        g_controllers[id].fd = fd;
    }
}

/* Vai trò: đóng hết socket đang mở của mọi controller và reset toàn bộ
 * state về ban đầu (UNINIT, fd = -1). */
void Can_DeInit(void) {
    for (uint8 i = 0U; i < CAN_MAX_CONTROLLERS; i++) {
        if (g_controllers[i].fd >= 0) {
            (void)close(g_controllers[i].fd);
        }
    }
    (void)memset(g_controllers, 0, sizeof(g_controllers));
    for (uint8 i = 0U; i < CAN_MAX_CONTROLLERS; i++) {
        g_controllers[i].fd = -1;
        g_controllers[i].state = CAN_CS_UNINIT;
    }
}

/* Vai trò: đổi trạng thái vận hành của 1 controller (STOPPED/STARTED/
 * SLEEP). Chỉ đổi cờ trạng thái trong phần mềm — không mở/đóng lại
 * socket (socket đã mở sẵn từ lúc Init, nếu Init thất bại thì không thể
 * "start" lại bằng cách gọi hàm này). */
Std_ReturnType Can_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode) {
    if ((controller_id >= CAN_MAX_CONTROLLERS) || (!g_controllers[controller_id].configured)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controller_id, CAN_API_ID_SET_CONTROLLER_MODE,
                              DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if ((mode != CAN_CS_STOPPED) && (mode != CAN_CS_STARTED) && (mode != CAN_CS_SLEEP)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controller_id, CAN_API_ID_SET_CONTROLLER_MODE,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if (g_controllers[controller_id].fd < 0) {
        /* The socket never opened; a controller that failed REQ-ECUM-001's
         * startup check cannot be started later just by asking. */
        (void)Det_ReportError(DET_MODULE_ID_CAN, controller_id, CAN_API_ID_SET_CONTROLLER_MODE,
                              DET_ERR_INVALID_CONFIG);
        return E_NOT_OK;
    }

    g_controllers[controller_id].state = mode;
    return E_OK;
}

/* Vai trò: đọc trạng thái hiện tại của 1 controller (getter thuần). */
Can_ControllerStateType Can_GetControllerMode(uint8 controller_id) {
    if (controller_id >= CAN_MAX_CONTROLLERS) {
        return CAN_CS_UNINIT;
    }
    return g_controllers[controller_id].state;
}

/* Vai trò: cho biết controller đã được cấu hình VÀ mở socket thành công
 * chưa — EcuM dùng hàm này lúc boot để quyết định có nên abort startup
 * không (REQ-ECUM-001). */
boolean Can_IsControllerReady(uint8 controller_id) {
    if (controller_id >= CAN_MAX_CONTROLLERS) {
        return FALSE;
    }
    return (g_controllers[controller_id].configured && (g_controllers[controller_id].fd >= 0))
               ? TRUE
               : FALSE;
}

/* Vai trò: gửi 1 frame CAN thật xuống socket — validate tham số, dựng
 * struct can_frame từ PduType, write() xuống kernel (non-blocking, có thể
 * fail vì hàng đợi Tx đầy — không coi là lỗi nghiêm trọng, chỉ đếm lại).
 * Gửi thành công thì gọi ngay CanIf_TxConfirmation (Tx ở project này là
 * đồng bộ, không cần đợi ngắt phần cứng). */
Std_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu_info) {
    if (hth >= (Can_HwHandleType)CAN_MAX_CONTROLLERS) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_WRITE, DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    uint8 controllerId = (uint8)hth;
    if (!g_controllers[controllerId].configured) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId, CAN_API_ID_WRITE, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if ((pdu_info == NULL) || (pdu_info->length > CAN_CLASSIC_MAX_DLC) ||
        ((pdu_info->length > 0U) && (pdu_info->sdu == NULL))) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId, CAN_API_ID_WRITE,
                              DET_ERR_INVALID_PARAM);
        g_controllers[controllerId].txFailureCount++;
        return E_NOT_OK;
    }
    if (g_controllers[controllerId].state != CAN_CS_STARTED) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId, CAN_API_ID_WRITE,
                              DET_ERR_INVALID_CONFIG);
        g_controllers[controllerId].txFailureCount++;
        return E_NOT_OK;
    }

    struct can_frame frame;
    (void)memset(&frame, 0, sizeof(frame));
    frame.can_id = pdu_info->id & CAN_SFF_MASK; /* standard 11-bit only (section 6.1) */
    frame.can_dlc = pdu_info->length;
    if (pdu_info->length > 0U) {
        (void)memcpy(frame.data, pdu_info->sdu, pdu_info->length);
    }

    ssize_t n = write(g_controllers[controllerId].fd, &frame, sizeof(frame));
    if (n != (ssize_t)sizeof(frame)) {
        /* EAGAIN (Tx queue full) or any other failure: non-fatal, counted,
         * never blocking -- same pattern as VirtualHw_Send. */
        g_controllers[controllerId].txFailureCount++;
        return E_NOT_OK;
    }

    /* Section 7.1 rule 9: "successful host transmission triggers the
     * simplified confirmation chain back to COM." PduR/COM do not exist
     * yet (Phase 4), so CanIf_TxConfirmation only counts this in Phase 3. */
    CanIf_TxConfirmation(pdu_info->swPduHandle);
    return E_OK;
}

/* Vai trò: hàm chạy theo chu kỳ (Task_Bsw_1ms) — với mỗi controller, đọc
 * dồn tối đa CAN_RX_PER_ACTIVATION frame từ socket (không block), lọc bỏ
 * error/remote frame và frame quá dài, rồi forward từng frame hợp lệ lên
 * CanIf_RxIndication(). Nếu controller chưa STARTED thì vẫn rút cạn
 * socket (tránh đầy buffer kernel) nhưng không báo lên trên — giống
 * hành vi 1 controller thật đang bus-off/stopped. */
void Can_MainFunction_Read(void) {
    for (uint8 controllerId = 0U; controllerId < CAN_MAX_CONTROLLERS; controllerId++) {
        if ((!g_controllers[controllerId].configured) || (g_controllers[controllerId].fd < 0)) {
            continue;
        }

        for (uint8 i = 0U; i < CAN_RX_PER_ACTIVATION; i++) {
            struct can_frame frame;
            ssize_t n = read(g_controllers[controllerId].fd, &frame, sizeof(frame));
            if (n < 0) {
                break; /* EAGAIN: nothing more waiting this cycle */
            }
            if (n != (ssize_t)sizeof(frame)) {
                g_controllers[controllerId].rxDroppedCount++;
                (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId,
                                      CAN_API_ID_MAIN_FUNCTION_READ, DET_ERR_INVALID_PARAM);
                continue;
            }
            if ((frame.can_id & (CAN_ERR_FLAG | CAN_RTR_FLAG)) != 0U) {
                /* Error frame or remote frame: this project only carries
                 * classic data frames (section 6.1). */
                g_controllers[controllerId].rxDroppedCount++;
                continue;
            }
            if (frame.can_dlc > CAN_CLASSIC_MAX_DLC) {
                g_controllers[controllerId].rxDroppedCount++;
                (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId,
                                      CAN_API_ID_MAIN_FUNCTION_READ, DET_ERR_INVALID_PARAM);
                continue;
            }

            g_controllers[controllerId].rxFrameCount++;

            if (g_controllers[controllerId].state != CAN_CS_STARTED) {
                /* Drained so the kernel socket buffer cannot silently fill
                 * up, but not indicated upward -- matches a real
                 * bus-off/stopped controller producing no Rx interrupts. */
                continue;
            }

            Can_HwType hw;
            hw.CanId = frame.can_id & CAN_SFF_MASK;
            hw.Hoh = 0U; /* single receive object per controller; see Can.h */
            hw.ControllerId = controllerId;

            PduInfoType info;
            info.SduDataPtr = frame.data;
            info.SduLength = frame.can_dlc;

            CanIf_RxIndication(&hw, &info);
        }
    }
}

/* Vai trò: dự phòng cho kiến trúc AUTOSAR chuẩn (Tx theo polling định kỳ)
 * — không làm gì cả ở đây vì project này gửi đồng bộ ngay trong
 * Can_Write(), không cần đợi chu kỳ Tx riêng. */
void Can_MainFunction_Write(void) {
    /* Reserved; see Can.h's doc comment. */
}

/* Vai trò: getter — tổng số frame đã đọc thành công từ socket. */
uint32 Can_GetRxFrameCount(uint8 controller_id) {
    return (controller_id < CAN_MAX_CONTROLLERS) ? g_controllers[controller_id].rxFrameCount : 0U;
}

/* Vai trò: getter — số frame bị loại bỏ (sai định dạng/error/remote/quá
 * dài) khi đọc từ socket. */
uint32 Can_GetRxDroppedCount(uint8 controller_id) {
    return (controller_id < CAN_MAX_CONTROLLERS) ? g_controllers[controller_id].rxDroppedCount : 0U;
}

/* Vai trò: getter — số lần Can_Write thất bại (validate lỗi, controller
 * chưa STARTED, hoặc write() xuống kernel thất bại). */
uint32 Can_GetTxFailureCount(uint8 controller_id) {
    return (controller_id < CAN_MAX_CONTROLLERS) ? g_controllers[controller_id].txFailureCount : 0U;
}
