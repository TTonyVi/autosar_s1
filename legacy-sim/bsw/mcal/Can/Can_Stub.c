/**
 * @file Can_Stub.c
 * @brief Microcontroller-MCAL stand-in for Can.c (REQ-ARCH-012).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 14.3: "Prove REQ-ARCH-012
 * by building both ECUs against a stub MCAL that replaces the links with
 * in-memory buffers, with no change outside bsw/mcal/."
 *
 * This file implements the exact same Can.h contract as Can.c, so every
 * file that includes Can.h -- CanIf.c, both ECUs' *_Cfg.c -- needs zero
 * changes to link against it instead. It contains no `socket`,
 * `AF_CAN`/`PF_CAN`, or `clock_gettime` call anywhere (REQ-ARCH-009 is
 * unaffected either way, since both variants live under bsw/mcal/).
 *
 * What "in-memory buffer" means here: a bounded, per-controller loopback
 * queue -- what this controller writes, only this SAME process's own
 * `Can_MainFunction_Read` ever drains. On real silicon this file is what
 * gets replaced by code that talks to an actual CAN peripheral; the point
 * of this exercise is that NOTHING else in the repository needs to know
 * that happened. It does not attempt to let two separate SensorEcu/
 * ClusterEcu OS processes exchange frames -- that would require the very
 * IPC mechanism (a socket) this exercise exists to prove is confined to
 * bsw/mcal/ in the first place. Build-and-link success, plus this driver
 * behaving correctly in isolation, is the claim being proven.
 *
 * CMakeLists.txt selects this file INSTEAD OF Can.c when configured with
 * -DAUTOSAR_MCAL_VARIANT=stub; the two are never compiled into the same
 * target.
 */
#include "Can.h"

#include <stddef.h>
#include <string.h>

#include "CanIf.h"
#include "Det.h"
#include "Det_ModuleIds.h"

#define CAN_API_ID_INIT ((Det_ApiIdType)1U)
#define CAN_API_ID_SET_CONTROLLER_MODE ((Det_ApiIdType)2U)
#define CAN_API_ID_WRITE ((Det_ApiIdType)3U)
#define CAN_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)4U)

#define CAN_MAX_CONTROLLERS ((uint8)2U)
#define CAN_LOOPBACK_QUEUE_DEPTH ((uint8)16U)
#define CAN_RX_PER_ACTIVATION ((uint8)8U)

typedef struct {
    Can_IdType id;
    uint8 dlc;
    uint8 data[CAN_CLASSIC_MAX_DLC];
} Can_QueuedFrameType;

typedef struct {
    boolean configured;
    Can_ControllerStateType state;
    Can_QueuedFrameType queue[CAN_LOOPBACK_QUEUE_DEPTH];
    uint8 queueHead;
    uint8 queueCount;
    uint32 rxFrameCount;
    uint32 rxDroppedCount;
    uint32 txFailureCount;
} Can_ControllerStateEntryType;

static Can_ControllerStateEntryType g_controllers[CAN_MAX_CONTROLLERS];
static boolean g_initialized;

/* Vai trò: kiểm tra controller_id hợp lệ + đã init + đã configured. */
static boolean ControllerIsValid(uint8 controller_id) {
    return g_initialized && (controller_id < CAN_MAX_CONTROLLERS) &&
           g_controllers[controller_id].configured;
}

/* Vai trò: giống Can_Init thật nhưng không mở socket — chỉ validate
 * config rồi đánh dấu controller "configured" (không có phần cứng để mở
 * nên luôn thành công nếu config hợp lệ). */
void Can_Init(const Can_ConfigType *config) {
    g_initialized = FALSE;
    (void)memset(g_controllers, 0, sizeof(g_controllers));

    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if ((config->controllers == NULL) || (config->numControllers == 0U) ||
        (config->numControllers > CAN_MAX_CONTROLLERS)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    for (uint8 i = 0U; i < config->numControllers; i++) {
        uint8 id = config->controllers[i].controllerId;
        if (id >= CAN_MAX_CONTROLLERS) {
            (void)Det_ReportError(DET_MODULE_ID_CAN, id, CAN_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            return;
        }
        if (!config->controllers[i].nonBlocking) {
            (void)Det_ReportError(DET_MODULE_ID_CAN, id, CAN_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            return;
        }
        /* No hardware to open on this variant -- "ready" is unconditional,
         * matching what a real microcontroller port's happy path would
         * report once its own peripheral init succeeds. */
        g_controllers[id].configured = TRUE;
        g_controllers[id].state = CAN_CS_STOPPED;
    }

    g_initialized = TRUE;
}

/* Vai trò: reset toàn bộ state stub về ban đầu (xoá cả hàng đợi loopback). */
void Can_DeInit(void) {
    g_initialized = FALSE;
    (void)memset(g_controllers, 0, sizeof(g_controllers));
}

/* Vai trò: đổi trạng thái controller (STOPPED/STARTED/SLEEP), giống hệt
 * Can.c thật vì đây thuần là cờ phần mềm, không liên quan phần cứng. */
Std_ReturnType Can_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode) {
    if (!ControllerIsValid(controller_id)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controller_id, CAN_API_ID_SET_CONTROLLER_MODE,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    g_controllers[controller_id].state = mode;
    return E_OK;
}

/* Vai trò: getter trạng thái controller. */
Can_ControllerStateType Can_GetControllerMode(uint8 controller_id) {
    return ControllerIsValid(controller_id) ? g_controllers[controller_id].state : CAN_CS_UNINIT;
}

/* Vai trò: "sẵn sàng" ở bản stub chỉ còn nghĩa "đã configured" — không có
 * phần cứng thật để mở/fail. */
boolean Can_IsControllerReady(uint8 controller_id) {
    /* No hardware to fail opening on this variant, so "ready" reduces to
     * "was configured" -- REQ-ECUM-001's failure path (a real interface
     * that could not be opened) has no equivalent stub failure mode to
     * simulate here; see the file header. */
    return ControllerIsValid(controller_id);
}

/* Vai trò: thay vì write() xuống socket, đẩy frame vào 1 hàng đợi vòng
 * (ring buffer) trong bộ nhớ của controller — chỉ chính process này tự
 * đọc lại qua Can_MainFunction_Read (loopback nội bộ, không phải IPC
 * thật giữa 2 ECU). Hàng đợi đầy thì coi như lỗi Tx, không block. */
Std_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu_info) {
    uint8 controllerId = (uint8)hth;
    if ((pdu_info == NULL) || (pdu_info->sdu == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId, CAN_API_ID_WRITE,
                              DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }
    if (!ControllerIsValid(controllerId)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId, CAN_API_ID_WRITE,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if ((pdu_info->length > CAN_CLASSIC_MAX_DLC) ||
        (g_controllers[controllerId].state != CAN_CS_STARTED)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, controllerId, CAN_API_ID_WRITE,
                              DET_ERR_INVALID_PARAM);
        g_controllers[controllerId].txFailureCount++;
        return E_NOT_OK;
    }

    Can_ControllerStateEntryType *ctrl = &g_controllers[controllerId];
    if (ctrl->queueCount >= CAN_LOOPBACK_QUEUE_DEPTH) {
        /* Bounded queue full -- matches a real transceiver reporting
         * "busy"; never blocks (section 8.1). */
        ctrl->txFailureCount++;
        return E_NOT_OK;
    }

    uint8 slot = (uint8)((ctrl->queueHead + ctrl->queueCount) % CAN_LOOPBACK_QUEUE_DEPTH);
    ctrl->queue[slot].id = pdu_info->id;
    ctrl->queue[slot].dlc = pdu_info->length;
    (void)memcpy(ctrl->queue[slot].data, pdu_info->sdu, pdu_info->length);
    ctrl->queueCount++;

    CanIf_TxConfirmation(pdu_info->swPduHandle);
    return E_OK;
}

/* Vai trò: rút frame ra khỏi hàng đợi loopback (thay vì đọc socket) và
 * forward lên CanIf_RxIndication — logic lọc/đếm giống hệt Can.c thật. */
void Can_MainFunction_Read(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, CAN_API_ID_MAIN_FUNCTION_READ, DET_ERR_UNINIT);
        return;
    }

    for (uint8 c = 0U; c < CAN_MAX_CONTROLLERS; c++) {
        Can_ControllerStateEntryType *ctrl = &g_controllers[c];
        if (!ctrl->configured) {
            continue;
        }

        uint8 drained = 0U;
        while ((ctrl->queueCount > 0U) && (drained < CAN_RX_PER_ACTIVATION)) {
            Can_QueuedFrameType frame = ctrl->queue[ctrl->queueHead];
            ctrl->queueHead = (uint8)((ctrl->queueHead + 1U) % CAN_LOOPBACK_QUEUE_DEPTH);
            ctrl->queueCount--;
            drained++;

            /* Matches Can.c: every drained (well-formed) frame counts here,
             * whether or not the controller is currently started -- this
             * is a "frame reached the driver" counter, not a "frame was
             * indicated upward" counter. */
            ctrl->rxFrameCount++;

            if (ctrl->state != CAN_CS_STARTED) {
                /* Drained so the queue cannot fill while stopped, but not
                 * indicated -- matches a real bus-off/stopped controller
                 * producing no Rx interrupts. Not counted as "dropped":
                 * that counter is reserved for malformed frames, which
                 * this loopback stub -- unlike real SocketCAN -- can
                 * never receive, since it only ever replays what
                 * Can_Write itself already validated. */
                continue;
            }

            Can_HwType hw;
            hw.CanId = frame.id;
            hw.Hoh = 0U; /* single receive object per controller; see Can.h */
            hw.ControllerId = c;

            PduInfoType info;
            info.SduDataPtr = frame.data;
            info.SduLength = frame.dlc;

            CanIf_RxIndication(&hw, &info);
        }
    }
}

/* Vai trò: dự phòng, không làm gì — giống Can.c thật. */
void Can_MainFunction_Write(void) {
    /* Reserved, same as the real driver -- see Can.h. */
}

/* Vai trò: getter số frame đã rút khỏi hàng đợi loopback thành công. */
uint32 Can_GetRxFrameCount(uint8 controller_id) {
    return ControllerIsValid(controller_id) ? g_controllers[controller_id].rxFrameCount : 0U;
}

/* Vai trò: getter số frame bị drop — ở bản stub luôn là 0 vì hàng đợi
 * chỉ chứa frame hợp lệ do chính Can_Write tạo ra (không có input hỏng
 * kiểu SocketCAN thật). */
uint32 Can_GetRxDroppedCount(uint8 controller_id) {
    return ControllerIsValid(controller_id) ? g_controllers[controller_id].rxDroppedCount : 0U;
}

/* Vai trò: getter số lần Can_Write thất bại (hàng đợi đầy/tham số sai). */
uint32 Can_GetTxFailureCount(uint8 controller_id) {
    return ControllerIsValid(controller_id) ? g_controllers[controller_id].txFailureCount : 0U;
}
