/**
 * @file Rte_SensorEcu.c
 * @brief Generated-style RTE implementation for SensorEcu.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 5.
 *
 * This file is the whole reason a SW-C can stay ignorant: it owns the
 * sender/receiver buffers, maps ports onto COM signal IDs
 * (`REQ-RTE-004`), turns COM reception health into an `Rte_ReturnType`,
 * and adapts the client-server port onto `IoHwAb` so the SW-C never names
 * an abstraction module.
 */
#include "Rte_SensorEcu.h"

#include <stddef.h>

#include "Com.h"
#include "Dem.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "IoHwAb.h"
#include "Rte_SensorEcu_Cfg.h"
#include "Rte_Type.h"
#include "Rte_VehicleStateSwc.h"
#include "VehicleStateSwc.h"

#define RTE_API_ID_READ ((Det_ApiIdType)1U)
#define RTE_API_ID_WRITE ((Det_ApiIdType)2U)
#define RTE_API_ID_CALL ((Det_ApiIdType)3U)
#define RTE_API_ID_START ((Det_ApiIdType)4U)
#define RTE_API_ID_TASK ((Det_ApiIdType)5U)

/* --- RTE-owned buffers. Reads copy out of these; nothing outside this
 *     file ever sees a pointer into them (REQ-RTE-002). --- */
static VehicleSpeedRawType g_txVehicleSpeedRaw;
static IgnitionOnType g_txIgnitionOn;

static ClusterStateType g_rxClusterState;
static DisplayedSpeedRawType g_rxDisplayedSpeedRaw;
static ClusterStatusFlagsType g_rxStatusFlags;

static boolean g_started;
static boolean g_everReceived;     /* REQ-RTE-006 */
static boolean g_lastFrameInvalid; /* last reception failed integrity */
static uint32 g_integrityErrorSnapshot;
static uint32 g_validRxSnapshot;
static uint32 g_rxUpdateCount;
static uint32 g_txWriteCount;

/** Vai trò: dịch trạng thái nhận ClusterStatus hiện tại (chưa từng nhận /
 * timeout / lỗi toàn vẹn / ok) sang đúng 1 mã Rte_ReturnType — SW-C chỉ
 * cần đọc mã này, không cần biết gì về Com bên dưới.
 * Current health of the ClusterStatus reception, as an Rte_ReturnType. */
static Rte_ReturnType RemoteStatus(void) {
    if (!g_everReceived) {
        return RTE_E_NEVER_RECEIVED; /* REQ-RTE-006 */
    }

    boolean timedOut = FALSE;
    /* Returns boolean TRUE on success, not E_OK — comparing against
     * E_OK (which is 0) would silently disable timeout detection. */
    if (Com_GetIpduRxTimeoutStatus((Com_IPduIdType)RTE_IPDU_RX_CLUSTER_STATUS, &timedOut)) {
        if (timedOut) {
            /* A deadline miss outranks an integrity failure: nothing is
             * arriving at all, which is the more severe condition. */
            return RTE_E_TIMEOUT;
        }
    }
    if (g_lastFrameInvalid) {
        return RTE_E_INVALID;
    }
    return RTE_E_OK;
}

/**
 * Vai trò: chạy mỗi chu kỳ (10ms task) — so sánh counter lỗi/counter hợp
 * lệ của Com với snapshot lần trước để suy ra "frame gần nhất có lỗi
 * toàn vẹn không", rồi set 2 DEM event (CLUSTER_STATUS_INTEGRITY,
 * CLUSTER_STATUS_TIMEOUT) tương ứng — đây là chỗ DUY NHẤT các event tầng
 * COM này được ghi nhận.
 *
 * Polls COM's reception counters once per cycle to decide whether the most
 * recent frame failed integrity validation. COM already refuses to update
 * signals for such a frame (`REQ-COM-003`, `REQ-RTE-005`); this only turns
 * that fact into the status the SW-C sees.
 */
static void UpdateReceptionHealth(void) {
    Com_IPduIdType ipdu = (Com_IPduIdType)RTE_IPDU_RX_CLUSTER_STATUS;
    uint32 integrityErrors = Com_GetIpduRxCrcErrorCount(ipdu) +
                             Com_GetIpduRxCounterErrorCount(ipdu) +
                             Com_GetIpduRxSignalRangeErrorCount(ipdu);
    uint32 validRx = Com_GetIpduRxValidCount(ipdu);

    if (integrityErrors > g_integrityErrorSnapshot) {
        g_lastFrameInvalid = TRUE;
    }
    if (validRx > g_validRxSnapshot) {
        /* A frame that passed every check clears the condition. */
        g_lastFrameInvalid = FALSE;
    }
    g_integrityErrorSnapshot = integrityErrors;
    g_validRxSnapshot = validRx;

    /* Section 10.2: "Sensor COM protection"/"Sensor COM/Rx supervision"
     * are this ECU's own DEM events for its ClusterStatus reception --
     * this is the layer boundary the table names, and the only place
     * that already computes both conditions every cycle. */
    (void)Dem_SetEventStatus(DEM_EVENT_CLUSTER_STATUS_INTEGRITY, g_lastFrameInvalid
                                                                     ? DEM_EVENT_STATUS_FAILED
                                                                     : DEM_EVENT_STATUS_PASSED);

    boolean timedOut = FALSE;
    /* Returns boolean TRUE on success, not E_OK -- see the RemoteStatus() comment above. */
    if (Com_GetIpduRxTimeoutStatus(ipdu, &timedOut)) {
        (void)Dem_SetEventStatus(DEM_EVENT_CLUSTER_STATUS_TIMEOUT,
                                 timedOut ? DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED);
    }
}

/* ------------------------- sender port ------------------------------- */

/* Vai trò: SW-C gọi để "xuất" tốc độ qua port gửi — RTE lưu bản sao nội
 * bộ rồi chuyển thành Com_SendSignal (SW-C không tự gọi Com). */
Rte_ReturnType Rte_Write_PpVehicleState_VehicleSpeedRaw(VehicleSpeedRawType value) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_WRITE, DET_ERR_UNINIT);
        return RTE_E_NOT_OK;
    }
    /* REQ-RTE-004: update the RTE value, then hand it to COM. */
    g_txVehicleSpeedRaw = value;
    uint16 comValue = (uint16)value;
    g_txWriteCount++;
    return (Com_SendSignal((Com_SignalIdType)RTE_SIGNAL_TX_VEHICLE_SPEED_RAW, &comValue) == E_OK)
               ? RTE_E_OK
               : RTE_E_NOT_OK;
}

/* Vai trò: cùng mẫu với Rte_Write_PpVehicleState_VehicleSpeedRaw, cho
 * tín hiệu IgnitionOn. */
Rte_ReturnType Rte_Write_PpVehicleState_IgnitionOn(IgnitionOnType value) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_WRITE, DET_ERR_UNINIT);
        return RTE_E_NOT_OK;
    }
    g_txIgnitionOn = value;
    uint16 comValue = (value != FALSE) ? 1U : 0U;
    g_txWriteCount++;
    return (Com_SendSignal((Com_SignalIdType)RTE_SIGNAL_TX_IGNITION_ON, &comValue) == E_OK)
               ? RTE_E_OK
               : RTE_E_NOT_OK;
}

/* ------------------------ receiver port ------------------------------ */

/* Vai trò: SW-C gọi để "nhập" ClusterState qua port nhận — trả về bản
 * sao từ buffer RTE (REQ-RTE-002, không lộ con trỏ nội bộ) kèm mã sức
 * khoẻ đường nhận (RemoteStatus). */
Rte_ReturnType Rte_Read_RpClusterStatus_ClusterState(ClusterStateType *value) {
    if (value == NULL) { /* REQ-RTE-001 */
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_READ, DET_ERR_NULL_POINTER);
        return RTE_E_NOT_OK;
    }
    *value = g_rxClusterState; /* REQ-RTE-002: copy, never expose the buffer */
    return RemoteStatus();
}

/* Vai trò: cùng mẫu với Rte_Read_RpClusterStatus_ClusterState, cho
 * DisplayedSpeedRaw. */
Rte_ReturnType Rte_Read_RpClusterStatus_DisplayedSpeedRaw(DisplayedSpeedRawType *value) {
    if (value == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_READ, DET_ERR_NULL_POINTER);
        return RTE_E_NOT_OK;
    }
    *value = g_rxDisplayedSpeedRaw;
    return RemoteStatus();
}

/* Vai trò: cùng mẫu với Rte_Read_RpClusterStatus_ClusterState, cho
 * StatusFlags. */
Rte_ReturnType Rte_Read_RpClusterStatus_StatusFlags(ClusterStatusFlagsType *value) {
    if (value == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_READ, DET_ERR_NULL_POINTER);
        return RTE_E_NOT_OK;
    }
    *value = g_rxStatusFlags;
    return RemoteStatus();
}

/* ------------------------- client port ------------------------------- */

/* Vai trò: adapter port client-server — SW-C gọi hàm này để lấy dữ liệu
 * cảm biến, RTE bên trong gọi IoHwAb_GetVehicleSpeedRaw +
 * IoHwAb_GetIgnitionState (SW-C không bao giờ tự tên IoHwAb — REQ-ARCH-
 * 008), gộp 2 cờ validity thành 1 verdict duy nhất cho cả cặp. */
Rte_ReturnType Rte_Call_RpVehicleInputs_GetVehicleInputs(VehicleSpeedRawType *speed_raw,
                                                         IgnitionOnType *ignition_on,
                                                         Rte_InputValidityType *validity) {
    if ((speed_raw == NULL) || (ignition_on == NULL) || (validity == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_CALL, DET_ERR_NULL_POINTER);
        return RTE_E_NOT_OK;
    }

    IoHwAb_ValidityType speedValidity = IOHWAB_INVALID;
    IoHwAb_ValidityType ignitionValidity = IOHWAB_INVALID;
    Std_ReturnType speedResult = IoHwAb_GetVehicleSpeedRaw(speed_raw, &speedValidity);
    Std_ReturnType ignitionResult = IoHwAb_GetIgnitionState(ignition_on, &ignitionValidity);

    if ((speedResult != E_OK) || (ignitionResult != E_OK)) {
        *speed_raw = 0U;
        *ignition_on = FALSE;
        *validity = RTE_INPUT_INVALID;
        return RTE_E_NOT_OK;
    }

    /* One validity for the whole service: either input being untrustworthy
     * makes the pair untrustworthy. `Rte_InputValidityType` mirrors
     * `IoHwAb_ValidityType` one to one (section 5.2). */
    if ((speedValidity != IOHWAB_VALID) || (ignitionValidity != IOHWAB_VALID)) {
        *validity = RTE_INPUT_INVALID;
        return RTE_E_HARDWARE_INVALID;
    }

    *validity = RTE_INPUT_VALID;
    return RTE_E_OK;
}

/* --------------------- COM receive notifications ---------------------
 * COM calls these only after full PDU integrity validation
 * (`REQ-RTE-005`), and only from the COM main function — never
 * re-entrantly while a runnable is executing. That is what makes the
 * multi-element update atomic from this single-threaded ECU's point of
 * view (`REQ-RTE-003`). */

/* Vai trò: callback Com gọi khi nhận được signal ClusterState mới (đã
 * qua hết kiểm tra toàn vẹn) — cập nhật buffer RTE nội bộ. */
void Rte_COMCbk_ClusterState(void) {
    uint16 value = 0U;
    if (Com_ReceiveSignal((Com_SignalIdType)RTE_SIGNAL_RX_CLUSTER_STATE, &value) == E_OK) {
        g_rxClusterState = (ClusterStateType)value;
        g_everReceived = TRUE;
        g_rxUpdateCount++;
    }
}

/* Vai trò: cùng mẫu với Rte_COMCbk_ClusterState, cho DisplayedSpeedRaw. */
void Rte_COMCbk_DisplayedSpeedRaw(void) {
    uint16 value = 0U;
    if (Com_ReceiveSignal((Com_SignalIdType)RTE_SIGNAL_RX_DISPLAYED_SPEED_RAW, &value) == E_OK) {
        g_rxDisplayedSpeedRaw = (DisplayedSpeedRawType)value;
        g_everReceived = TRUE;
        g_rxUpdateCount++;
    }
}

/* Vai trò: cùng mẫu với Rte_COMCbk_ClusterState, cho StatusFlags. */
void Rte_COMCbk_StatusFlags(void) {
    uint16 value = 0U;
    if (Com_ReceiveSignal((Com_SignalIdType)RTE_SIGNAL_RX_STATUS_FLAGS, &value) == E_OK) {
        g_rxStatusFlags = (ClusterStatusFlagsType)value;
        g_everReceived = TRUE;
        g_rxUpdateCount++;
    }
}

/* --------------------------- lifecycle ------------------------------- */

/* Vai trò: EcuM gọi lúc khởi động (bước 16, sau khi Com/PduR/CanIf đã
 * lên) — reset toàn bộ buffer RTE, bật cờ started, rồi gọi
 * VehicleStateSwc_Init() (RTE là lớp DUY NHẤT được phép gọi vào SW-C). */
Std_ReturnType Rte_Start(void) {
    g_txVehicleSpeedRaw = 0U;
    g_txIgnitionOn = FALSE;
    g_rxClusterState = CLUSTER_STATE_COMM_FAULT;
    g_rxDisplayedSpeedRaw = 0U;
    g_rxStatusFlags = 0U;
    g_everReceived = FALSE; /* REQ-RTE-006 */
    g_lastFrameInvalid = FALSE;
    g_integrityErrorSnapshot = 0U;
    g_validRxSnapshot = 0U;
    g_rxUpdateCount = 0U;
    g_txWriteCount = 0U;
    g_started = TRUE;

    VehicleStateSwc_Init();
    return E_OK;
}

/* Vai trò: EcuM gọi lúc shutdown — chỉ hạ cờ started, mọi Rte_Write/Read
 * sau đó sẽ từ chối. */
Std_ReturnType Rte_Stop(void) {
    g_started = FALSE;
    return E_OK;
}

/* Vai trò: Os gọi mỗi 10ms — cập nhật sức khoẻ đường nhận rồi gọi runnable
 * 10ms của SW-C (đúng thứ tự: health trước, SW-C sau, để SW-C luôn thấy
 * dữ liệu tình trạng mới nhất trong cùng 1 chu kỳ). */
void Rte_Task_10ms(void) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_TASK, DET_ERR_UNINIT);
        return;
    }
    UpdateReceptionHealth();
    VehicleStateSwc_10ms();
}

/* Vai trò: Os gọi mỗi 100ms — gọi runnable 100ms của SW-C. */
void Rte_Task_100ms(void) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_TASK, DET_ERR_UNINIT);
        return;
    }
    VehicleStateSwc_100ms();
}

/* Vai trò: getter — số lần buffer nhận đã được callback COM cập nhật. */
uint32 Rte_GetRxUpdateCount(void) {
    return g_rxUpdateCount;
}

/* Vai trò: getter — số lần SW-C đã ghi (Write) ra port gửi. */
uint32 Rte_GetTxWriteCount(void) {
    return g_txWriteCount;
}
