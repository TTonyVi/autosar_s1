/**
 * @file Rte_ClusterEcu.c
 * @brief Generated-style RTE implementation for ClusterEcu.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 5. Mirror of
 * `Rte_SensorEcu.c`: same responsibilities, opposite direction, and a
 * display client port instead of an input client port.
 */
#include "Rte_ClusterEcu.h"

#include <stddef.h>

#include "ClusterControlSwc.h"
#include "Com.h"
#include "Dem.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "IoHwAb.h"
#include "Rte_ClusterControlSwc.h"
#include "Rte_ClusterEcu_Cfg.h"
#include "Rte_Type.h"

#define RTE_API_ID_READ ((Det_ApiIdType)1U)
#define RTE_API_ID_WRITE ((Det_ApiIdType)2U)
#define RTE_API_ID_CALL ((Det_ApiIdType)3U)
#define RTE_API_ID_START ((Det_ApiIdType)4U)
#define RTE_API_ID_TASK ((Det_ApiIdType)5U)

static ClusterStateType g_txClusterState;
static DisplayedSpeedRawType g_txDisplayedSpeedRaw;
static ClusterStatusFlagsType g_txStatusFlags;

static VehicleSpeedRawType g_rxVehicleSpeedRaw;
static IgnitionOnType g_rxIgnitionOn;

static boolean g_started;
static boolean g_everReceived;     /* REQ-RTE-006 */
static boolean g_lastFrameInvalid; /* last reception failed integrity */
static uint32 g_integrityErrorSnapshot;
static uint32 g_validRxSnapshot;
static uint32 g_rxUpdateCount;
static uint32 g_txWriteCount;

/* Vai trò: giống RemoteStatus của Rte_SensorEcu.c — dịch trạng thái nhận
 * VehicleStatus thành 1 mã Rte_ReturnType. */
static Rte_ReturnType RemoteStatus(void) {
    if (!g_everReceived) {
        return RTE_E_NEVER_RECEIVED;
    }

    boolean timedOut = FALSE;
    /* Returns boolean TRUE on success, not E_OK — comparing against
     * E_OK (which is 0) would silently disable timeout detection. */
    if (Com_GetIpduRxTimeoutStatus((Com_IPduIdType)RTE_IPDU_RX_VEHICLE_STATUS, &timedOut)) {
        if (timedOut) {
            return RTE_E_TIMEOUT;
        }
    }
    if (g_lastFrameInvalid) {
        return RTE_E_INVALID;
    }
    return RTE_E_OK;
}

/* Vai trò: giống UpdateReceptionHealth của Rte_SensorEcu.c — set
 * DEM_EVENT_VEHICLE_STATUS_INTEGRITY/TIMEOUT mỗi chu kỳ dựa trên counter
 * lỗi của Com. */
static void UpdateReceptionHealth(void) {
    Com_IPduIdType ipdu = (Com_IPduIdType)RTE_IPDU_RX_VEHICLE_STATUS;
    uint32 integrityErrors = Com_GetIpduRxCrcErrorCount(ipdu) +
                             Com_GetIpduRxCounterErrorCount(ipdu) +
                             Com_GetIpduRxSignalRangeErrorCount(ipdu);
    uint32 validRx = Com_GetIpduRxValidCount(ipdu);

    if (integrityErrors > g_integrityErrorSnapshot) {
        g_lastFrameInvalid = TRUE;
    }
    if (validRx > g_validRxSnapshot) {
        g_lastFrameInvalid = FALSE;
    }
    g_integrityErrorSnapshot = integrityErrors;
    g_validRxSnapshot = validRx;

    /* Section 10.2: "Cluster COM protection"/"Cluster COM/Rx supervision"
     * are this ECU's own DEM events for its VehicleStatus reception. */
    (void)Dem_SetEventStatus(DEM_EVENT_VEHICLE_STATUS_INTEGRITY, g_lastFrameInvalid
                                                                     ? DEM_EVENT_STATUS_FAILED
                                                                     : DEM_EVENT_STATUS_PASSED);

    boolean timedOut = FALSE;
    if (Com_GetIpduRxTimeoutStatus(ipdu, &timedOut)) {
        (void)Dem_SetEventStatus(DEM_EVENT_VEHICLE_STATUS_TIMEOUT,
                                 timedOut ? DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED);
    }
}

/* ------------------------ receiver port ------------------------------ */

/* Vai trò: SW-C gọi để "nhập" VehicleSpeedRaw qua port nhận. */
Rte_ReturnType Rte_Read_RpVehicleState_VehicleSpeedRaw(VehicleSpeedRawType *value) {
    if (value == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_READ, DET_ERR_NULL_POINTER);
        return RTE_E_NOT_OK;
    }
    *value = g_rxVehicleSpeedRaw;
    return RemoteStatus();
}

/* Vai trò: SW-C gọi để "nhập" IgnitionOn qua port nhận. */
Rte_ReturnType Rte_Read_RpVehicleState_IgnitionOn(IgnitionOnType *value) {
    if (value == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_READ, DET_ERR_NULL_POINTER);
        return RTE_E_NOT_OK;
    }
    *value = g_rxIgnitionOn;
    return RemoteStatus();
}

/* ------------------------- sender port ------------------------------- */

/* Vai trò: SW-C gọi để "xuất" ClusterState qua port gửi. */
Rte_ReturnType Rte_Write_PpClusterStatus_ClusterState(ClusterStateType value) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_WRITE, DET_ERR_UNINIT);
        return RTE_E_NOT_OK;
    }
    g_txClusterState = value;
    uint16 comValue = (uint16)value;
    g_txWriteCount++;
    return (Com_SendSignal((Com_SignalIdType)RTE_SIGNAL_TX_CLUSTER_STATE, &comValue) == E_OK)
               ? RTE_E_OK
               : RTE_E_NOT_OK;
}

/* Vai trò: SW-C gọi để "xuất" DisplayedSpeedRaw qua port gửi. */
Rte_ReturnType Rte_Write_PpClusterStatus_DisplayedSpeedRaw(DisplayedSpeedRawType value) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_WRITE, DET_ERR_UNINIT);
        return RTE_E_NOT_OK;
    }
    g_txDisplayedSpeedRaw = value;
    uint16 comValue = (uint16)value;
    g_txWriteCount++;
    return (Com_SendSignal((Com_SignalIdType)RTE_SIGNAL_TX_DISPLAYED_SPEED_RAW, &comValue) == E_OK)
               ? RTE_E_OK
               : RTE_E_NOT_OK;
}

/* Vai trò: SW-C gọi để "xuất" StatusFlags qua port gửi. */
Rte_ReturnType Rte_Write_PpClusterStatus_StatusFlags(ClusterStatusFlagsType value) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_WRITE, DET_ERR_UNINIT);
        return RTE_E_NOT_OK;
    }
    g_txStatusFlags = value;
    uint16 comValue = (uint16)value;
    g_txWriteCount++;
    return (Com_SendSignal((Com_SignalIdType)RTE_SIGNAL_TX_STATUS_FLAGS, &comValue) == E_OK)
               ? RTE_E_OK
               : RTE_E_NOT_OK;
}

/* ------------------------- client port ------------------------------- */

/* Vai trò: adapter port client-server — SW-C gọi hàm này để ghi màn
 * hình, RTE bên trong gọi IoHwAb_SetClusterDisplay (SW-C không tự tên
 * IoHwAb). Lỗi ghi được báo lại nhưng KHÔNG BAO GIỜ fatal (REQ-VHW-005). */
Rte_ReturnType Rte_Call_RpDisplay_SetClusterDisplay(ClusterStateType state,
                                                    DisplayedSpeedRawType displayed_speed_raw,
                                                    ClusterStatusFlagsType flags) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_CALL, DET_ERR_UNINIT);
        return RTE_E_NOT_OK;
    }

    IoHwAb_ClusterDisplayType value;
    value.state = state;
    value.displayedSpeedRaw = displayed_speed_raw;
    value.flags = flags;

    /* REQ-VHW-005: a failed write is reported to the caller but is never
     * fatal; the SW-C is required to ignore it. */
    return (IoHwAb_SetClusterDisplay(&value) == E_OK) ? RTE_E_OK : RTE_E_NOT_OK;
}

/* --------------------- COM receive notifications --------------------- */

/* Vai trò: callback Com gọi khi nhận được signal VehicleSpeedRaw mới. */
void Rte_COMCbk_VehicleSpeedRaw(void) {
    uint16 value = 0U;
    if (Com_ReceiveSignal((Com_SignalIdType)RTE_SIGNAL_RX_VEHICLE_SPEED_RAW, &value) == E_OK) {
        g_rxVehicleSpeedRaw = (VehicleSpeedRawType)value;
        g_everReceived = TRUE;
        g_rxUpdateCount++;
    }
}

/* Vai trò: callback Com gọi khi nhận được signal IgnitionOn mới. */
void Rte_COMCbk_IgnitionOn(void) {
    uint16 value = 0U;
    if (Com_ReceiveSignal((Com_SignalIdType)RTE_SIGNAL_RX_IGNITION_ON, &value) == E_OK) {
        g_rxIgnitionOn = (value != 0U) ? TRUE : FALSE;
        g_everReceived = TRUE;
        g_rxUpdateCount++;
    }
}

/* --------------------------- lifecycle ------------------------------- */

/* Vai trò: EcuM gọi lúc khởi động — reset buffer, bật cờ started, gọi
 * ClusterControlSwc_Init(). */
Std_ReturnType Rte_Start(void) {
    g_txClusterState = CLUSTER_STATE_COMM_FAULT;
    g_txDisplayedSpeedRaw = 0U;
    g_txStatusFlags = 0U;
    g_rxVehicleSpeedRaw = 0U;
    g_rxIgnitionOn = FALSE;
    g_everReceived = FALSE;
    g_lastFrameInvalid = FALSE;
    g_integrityErrorSnapshot = 0U;
    g_validRxSnapshot = 0U;
    g_rxUpdateCount = 0U;
    g_txWriteCount = 0U;
    g_started = TRUE;

    ClusterControlSwc_Init();
    return E_OK;
}

/* Vai trò: EcuM gọi lúc shutdown — hạ cờ started. */
Std_ReturnType Rte_Stop(void) {
    g_started = FALSE;
    return E_OK;
}

/* Vai trò: Os gọi mỗi 10ms — cập nhật sức khoẻ đường nhận rồi gọi
 * runnable 10ms của SW-C (state machine). */
void Rte_Task_10ms(void) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_TASK, DET_ERR_UNINIT);
        return;
    }
    UpdateReceptionHealth();
    ClusterControlSwc_10ms();
}

/* Vai trò: Os gọi mỗi 100ms — gọi runnable 100ms của SW-C (ghi màn hình). */
void Rte_Task_100ms(void) {
    if (!g_started) {
        (void)Det_ReportError(DET_MODULE_ID_RTE, 0U, RTE_API_ID_TASK, DET_ERR_UNINIT);
        return;
    }
    ClusterControlSwc_100ms();
}

/* Vai trò: getter — số lần buffer nhận đã được callback COM cập nhật. */
uint32 Rte_GetRxUpdateCount(void) {
    return g_rxUpdateCount;
}

/* Vai trò: getter — số lần SW-C đã ghi (Write) ra port gửi. */
uint32 Rte_GetTxWriteCount(void) {
    return g_txWriteCount;
}
