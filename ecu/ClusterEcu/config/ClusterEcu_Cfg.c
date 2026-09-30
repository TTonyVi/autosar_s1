/**
 * @file ClusterEcu_Cfg.c
 * @brief The ClusterEcu's static configuration and task bodies.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 6.2, 6.4, 7.5, 8.2, 11.
 *
 * Mirror of `SensorEcu_Cfg.c`: same module implementations, opposite
 * direction, a display line instead of a sensor line (`REQ-ARCH-005`).
 */
#include "ClusterEcu_Cfg.h"

#include <stddef.h>

#include "BswM.h"
#include "ClusterControlSwc.h"
#include "Com.h"
#include "Dem.h"
#include "Det.h"
#include "Logger.h"
#include "Rte_ClusterEcu.h"
#include "Rte_ClusterEcu_Cfg.h"

/* ------------------------- MCAL: base drivers ------------------------ */

static const Mcu_ConfigType g_mcuConfig = {.reserved = 0U};
static const Port_ConfigType g_portConfig = {.reserved = 0U};
static const Gpt_ChannelConfigType g_gptChannels[1] = {{.channelId = GPT_CHANNEL_TIME_BASE}};
static const Gpt_ConfigType g_gptConfig = {.channels = g_gptChannels, .numChannels = 1U};

/* DISPLAY_LINK_ID is a legacy grouping id carried in the shared
 * Display_ConfigType.virtualHwLinkId field (a plain uint8, not a
 * VirtualHw type) — Display_S32K144.c drives LPUART1 directly and
 * ignores it. */
#define DISPLAY_LINK_ID 0U

static const Display_ConfigType g_displayConfig = {
    .virtualHwLinkId = DISPLAY_LINK_ID,
    .maxFrameLength = DISPLAY_MAX_FRAME_LENGTH,
    /* The SW-C refreshes the panel every 100 ms; the driver's floor is set
     * below that so it paces rather than throttles normal traffic. */
    .minWritePeriodMs = 50U,
};

/* ------------------------- MCAL: CAN driver -------------------------- */
#define CLUSTER_CAN_CONTROLLER 0U

static Can_ControllerConfigType g_canControllers[1] = {{
    .controllerId = CLUSTER_CAN_CONTROLLER,
    .interfaceName = NULL, /* patched from the command line */
    .nonBlocking = TRUE,
}};
static const Can_ConfigType g_canConfig = {.controllers = g_canControllers, .numControllers = 1U};

/* --------------------- CanIf: section 6.2 and 7.5 -------------------- */

static const CanIf_TxPduConfigType g_canIfTxPdus[1] = {{
    .canIfTxPduId = 0U,
    .canId = 0x181U, /* ClusterStatus */
    .dlc = 8U,
    .hth = 0U,
    .controllerId = CLUSTER_CAN_CONTROLLER,
}};
static const CanIf_RxPduConfigType g_canIfRxPdus[1] = {{
    .canIfRxPduId = 0U,
    .canId = 0x180U, /* VehicleStatus */
    .dlc = 8U,
    .controllerId = CLUSTER_CAN_CONTROLLER,
}};
static const CanIf_ConfigType g_canIfConfig = {
    .txPdus = g_canIfTxPdus,
    .numTxPdus = 1U,
    .rxPdus = g_canIfRxPdus,
    .numRxPdus = 1U,
};

/* --------------------------- PduR: section 7.5 ----------------------- */

static const PduR_RouteType g_pduRRoutes[2] = {
    /* Tx: COM ClusterStatus -> CanIf Tx PDU 0 -> CAN 0x181 */
    {PDUR_MODULE_COM, 0U, PDUR_MODULE_CANIF, 0U},
    /* Rx: CAN 0x180 -> CanIf Rx PDU 0 -> COM VehicleStatus (I-PDU 1) */
    {PDUR_MODULE_CANIF, 0U, PDUR_MODULE_COM, 1U},
};
static const PduR_ConfigType g_pduRConfig = {.routes = g_pduRRoutes, .numRoutes = 2U};

/* ---------------- COM: sections 6.2, 6.3, 6.4 and 6.5 ---------------- */

static const Com_IPduConfigType g_comIpdus[RTE_NUM_IPDUS] = {
    {
        .ipduId = RTE_IPDU_TX_CLUSTER_STATUS,
        .direction = COM_IPDU_DIR_TX,
        .pduRPduId = RTE_IPDU_TX_CLUSTER_STATUS,
        .length = 8U,
        .periodMs = 100U, /* section 6.2 */
        .timeoutMs = 0U,
        .counterBitPosition = 32U, /* byte 4, bits 0..3 */
        .counterBitLength = 4U,
        .crcByteIndex = 5U,
        .protectionDataId = 0x0181U,
        .protectionEnabled = TRUE,
    },
    {
        .ipduId = RTE_IPDU_RX_VEHICLE_STATUS,
        .direction = COM_IPDU_DIR_RX,
        .pduRPduId = RTE_IPDU_RX_VEHICLE_STATUS,
        .length = 8U,
        .periodMs = 0U,
        .timeoutMs = 100U,         /* section 6.2 deadline for VehicleStatus */
        .counterBitPosition = 24U, /* byte 3, bits 0..3 */
        .counterBitLength = 4U,
        .crcByteIndex = 4U,
        .protectionDataId = 0x0180U,
        .protectionEnabled = TRUE,
    },
};

static const Com_SignalConfigType g_comSignals[RTE_NUM_SIGNALS] = {
    {RTE_SIGNAL_TX_CLUSTER_STATE, RTE_IPDU_TX_CLUSTER_STATUS, 0U, 8U, 3U, 0U, 3U, NULL},
    {RTE_SIGNAL_TX_DISPLAYED_SPEED_RAW, RTE_IPDU_TX_CLUSTER_STATUS, 8U, 16U, 0U, 0U, 25000U, NULL},
    {RTE_SIGNAL_TX_STATUS_FLAGS, RTE_IPDU_TX_CLUSTER_STATUS, 24U, 3U, 0U, 0U, 7U, NULL},
    {RTE_SIGNAL_RX_VEHICLE_SPEED_RAW, RTE_IPDU_RX_VEHICLE_STATUS, 0U, 16U, 0U, 0U, 25000U,
     Rte_COMCbk_VehicleSpeedRaw},
    {RTE_SIGNAL_RX_IGNITION_ON, RTE_IPDU_RX_VEHICLE_STATUS, 16U, 1U, 0U, 0U, 1U,
     Rte_COMCbk_IgnitionOn},
};

static const Com_ConfigType g_comConfig = {
    .ipdus = g_comIpdus,
    .numIpdus = (uint8)RTE_NUM_IPDUS,
    .signals = g_comSignals,
    .numSignals = (uint8)RTE_NUM_SIGNALS,
};

/* ------------------------- Os: section 8.2 --------------------------- */

/* Vai trò: thân task ưu tiên cao nhất, chu kỳ 1ms — chỉ đọc/ghi socket
 * CAN, giống hệt SensorEcu_Cfg.c. */
static void Task_Bsw_1ms(void) {
    Can_MainFunction_Read();
    Can_MainFunction_Write();
}

/* Vai trò: thân task chu kỳ 5ms — khác SensorEcu ở chỗ chạy
 * Display_MainFunction() (không phải Adc/Dio) rồi IoHwAb_MainFunction. */
static void Task_Io_5ms(void) {
    Display_MainFunction();
    IoHwAb_MainFunction();
}

static const char *const kStateNames[4] = {"OFF", "NORMAL", "OVERSPEED", "COMM_FAULT"};

/** Vai trò: chỉ ClusterEcu mới có — in log mỗi khi ClusterControlSwc CHUYỂN
 * trạng thái (OFF/NORMAL/OVERSPEED/COMM_FAULT), lần seed đầu tiên không
 * tính là "chuyển" để tránh in ngay khi vừa khởi động.
 * Section 15.8: every cluster state transition is logged with a
 *  timestamp. Transitions are rare by nature, so this stays within the
 *  bounded-volume rule of section 13.4 even though it sits in a 10 ms
 *  task — only a change emits a line. */
static void LogStateTransition(void) {
    static ClusterStateType previous = CLUSTER_STATE_COMM_FAULT;
    static boolean seeded;

    ClusterStateType current = ClusterControlSwc_GetState();
    if (!seeded) {
        previous = current;
        seeded = TRUE;
        return;
    }
    if (current != previous) {
        Logger_Log(LOGGER_LEVEL_INFO, "ClusterControlSwc", "state %s -> %s speed=%u flags=0x%02X",
                   kStateNames[previous], kStateNames[current],
                   (unsigned)ClusterControlSwc_GetDisplayedSpeedRaw(),
                   (unsigned)ClusterControlSwc_GetStatusFlags());
        previous = current;
    }
}

/* Vai trò: thân task chu kỳ 10ms — chạy runnable ứng dụng (state
 * machine) rồi kiểm tra log chuyển trạng thái. */
static void Task_App_10ms(void) {
    Rte_Task_10ms();
    LogStateTransition();
}

/* Vai trò: thân task chu kỳ 20ms — chạy Com Tx/Rx, giống SensorEcu_Cfg.c. */
static void Task_BswCom_20ms(void) {
    Com_MainFunctionTx();
    Com_MainFunctionRx();
}

/** Vai trò: giống hệt LogDemTransitions của SensorEcu_Cfg.c — chỉ in khi
 * trạng thái 1 event Dem vừa đổi.
 * Section 15.8: both ECUs log DEM transitions. Only a change in a
 *  qualified status emits a line, so the volume stays bounded. */
static void LogDemTransitions(void) {
    /* Seeded to PASSED, which is exactly the state a freshly initialized
     * Dem is in. Gating on a separate "first pass" flag instead would
     * swallow the very first transition of any event that is already
     * failed when it is first observed — which is the normal case for a
     * sensor line with nothing plugged into it. */
    static Dem_EventStatusType previous[DEM_EVENT_ID_MAX + 1U];

    for (Dem_EventIdType id = DEM_EVENT_ID_MIN; id <= DEM_EVENT_ID_MAX; id++) {
        Dem_EventRecordType record;
        if (Dem_GetEventRecord(id, &record) != E_OK) {
            continue;
        }
        if (!record.everReported) {
            continue;
        }
        if (record.currentStatus != previous[id]) {
            Logger_Log((record.currentStatus == DEM_EVENT_STATUS_FAILED) ? LOGGER_LEVEL_WARN
                                                                         : LOGGER_LEVEL_INFO,
                       "Dem", "event %u %s count=%u", (unsigned)id,
                       (record.currentStatus == DEM_EVENT_STATUS_FAILED) ? "failed" : "passed",
                       (unsigned)record.occurrenceCount);
        }
        previous[id] = record.currentStatus;
    }
}

/* Vai trò: giống LogStatus của SensorEcu_Cfg.c — 1 dòng trạng thái tổng
 * quát + 1 dòng "[Layers]" mỗi giây, phục vụ IT-04 xác nhận Rx side của
 * VehicleStatus đi qua đủ từng tầng (Can → CanIf → PduR → Com → Rte). */
static void LogStatus(void) {
    static uint8 tick;
    tick++;
    if (tick < 10U) {
        return;
    }
    tick = 0U;

    Dem_EventRecordType record;
    Dem_EventStatusType vehicleTimeout = DEM_EVENT_STATUS_PASSED;
    if (Dem_GetEventRecord(DEM_EVENT_VEHICLE_STATUS_TIMEOUT, &record) == E_OK) {
        vehicleTimeout = record.currentStatus;
    }

    Logger_Log(LOGGER_LEVEL_INFO, "ClusterEcu",
               "state=%u speed=%u flags=0x%02X rx_valid=%u vehicle_timeout=%u display_fail=%u",
               (unsigned)ClusterControlSwc_GetState(),
               (unsigned)ClusterControlSwc_GetDisplayedSpeedRaw(),
               (unsigned)ClusterControlSwc_GetStatusFlags(),
               (unsigned)Com_GetIpduRxValidCount((Com_IPduIdType)RTE_IPDU_RX_VEHICLE_STATUS),
               (unsigned)vehicleTimeout, (unsigned)IoHwAb_GetDisplayWriteFailureCount());

    /* IT-04 "Layered Tx/Rx evidence" for this ECU's Rx side of the same
     * VehicleStatus exchange SensorEcu's own "Layers" line reports the
     * Tx side of. Test instrumentation only, per the module docstring in
     * SensorEcu_Cfg.c. */
    Logger_Log(LOGGER_LEVEL_INFO, "Layers",
               "can_rx=%u canif_rx_accepted=%u pdur_rx_fwd=%u com_rx=%u rte_rx=%u",
               (unsigned)Can_GetRxFrameCount(CLUSTER_CAN_CONTROLLER),
               (unsigned)CanIf_GetRxAcceptedCount(),
               (unsigned)PduR_GetCanIfRxIndicationForwardCount(),
               (unsigned)Com_GetIpduRxValidCount((Com_IPduIdType)RTE_IPDU_RX_VEHICLE_STATUS),
               (unsigned)Rte_GetRxUpdateCount());
}

/* Vai trò: thân task chu kỳ 100ms — chạy runnable 100ms (ghi display),
 * tổng hợp Dem, in 2 dòng log định kỳ. */
static void Task_App_100ms(void) {
    Rte_Task_100ms();
    Dem_MainFunction();
    LogDemTransitions();
    LogStatus();
}

static const Os_TaskConfigType g_osTasks[5] = {
    {"Task_Bsw_1ms", 1U, 4U, Task_Bsw_1ms},       {"Task_Io_5ms", 5U, 3U, Task_Io_5ms},
    {"Task_App_10ms", 10U, 2U, Task_App_10ms},    {"Task_BswCom_20ms", 20U, 2U, Task_BswCom_20ms},
    {"Task_App_100ms", 100U, 1U, Task_App_100ms},
};

/* ----------------------- CanSM & ComM -------------------------------- */

static const CanSM_NetworkConfigType g_canSMNetworks[1] = {
    {.canControllerId = CLUSTER_CAN_CONTROLLER, .networkHandle = COMM_CHANNEL_CAN},
};
static const CanSM_ConfigType g_canSMConfig = {
    .networks = g_canSMNetworks,
    .numNetworks = 1U,
};

static const ComM_UserHandleType g_commUsers[1] = {COMM_USER_BSWM};
static const ComM_ChannelConfigType g_commChannels[1] = {
    {.channelId = COMM_CHANNEL_CAN, .numUsers = 1U, .users = g_commUsers},
};
static const ComM_ConfigType g_commConfig = {
    .channels = g_commChannels,
    .numChannels = 1U,
    .numUsers = 1U,
};

/* ----------------------- BswM & OS StartupHook ----------------------- */

static BswM_ConfigType g_bswMConfig;

static void ClusterEcu_StartupHook(void) {
    EcuM_StartupTwo();
    if (BswM_Init(&g_bswMConfig) != E_OK) {
        Logger_Log(LOGGER_LEVEL_ERROR, "ClusterEcu", "BswM_Init failed");
    }
}

static const Os_ConfigType g_osConfig = {
    .tasks = g_osTasks,
    .numTasks = 5U,
    .startupHook = ClusterEcu_StartupHook,
    .shutdownHook = NULL,
};

/* --------------------------- assembly -------------------------------- */

static EcuM_ConfigType g_ecuMConfig;

/* Vai trò: LẮP RÁP EcuM_ConfigType cho ClusterEcu — giống
 * SensorEcu_BuildConfig nhưng thêm 1 nhánh riêng: nếu hwLinkEnabled là
 * FALSE thì bỏ hẳn displayConfig (NULL) — cluster có thể chạy hoàn toàn
 * không cần panel, chỉ log (section 15.4). */
const EcuM_ConfigType *ClusterEcu_BuildConfig(const EcuM_PostBuildType *postBuild) {
    if ((postBuild == NULL) || (postBuild->canInterface == NULL)) {
        return NULL;
    }

    g_canControllers[0].interfaceName = postBuild->canInterface;

    g_ecuMConfig.ecuName = "ClusterEcu";
    g_ecuMConfig.logLevel = postBuild->logLevel;
    g_ecuMConfig.durationSeconds = postBuild->durationSeconds;
    g_ecuMConfig.mcuConfig = &g_mcuConfig;
    g_ecuMConfig.portConfig = &g_portConfig;
    g_ecuMConfig.sysTimerConfig = &SysTimer_Config;
    g_ecuMConfig.gptConfig = &g_gptConfig;
    g_ecuMConfig.adcConfig = NULL; /* the cluster ECU has no local sensor */
    g_ecuMConfig.dioConfig = NULL;
    g_ecuMConfig.canConfig = &g_canConfig;
    g_ecuMConfig.ioHwAbConfig = &IoHwAb_ConfigCluster;
    g_ecuMConfig.canIfConfig = &g_canIfConfig;
    g_ecuMConfig.pduRConfig = &g_pduRConfig;
    g_ecuMConfig.comConfig = &g_comConfig;
    g_ecuMConfig.osConfig = &g_osConfig;
    g_ecuMConfig.canControllerId = CLUSTER_CAN_CONTROLLER;

    /* Section 15.4: hwLinkEnabled == FALSE disables the display driver and
     * the ECU logs only. */
    g_ecuMConfig.displayConfig = postBuild->hwLinkEnabled ? &g_displayConfig : NULL;

    /* Assemble BswM configuration */
    g_bswMConfig.adcConfig = NULL;
    g_bswMConfig.dioConfig = NULL;
    g_bswMConfig.displayConfig = g_ecuMConfig.displayConfig;
    g_bswMConfig.canConfig = &g_canConfig;
    g_bswMConfig.ioHwAbConfig = &IoHwAb_ConfigCluster;
    g_bswMConfig.canIfConfig = &g_canIfConfig;
    g_bswMConfig.canSMConfig = &g_canSMConfig;
    g_bswMConfig.pduRConfig = &g_pduRConfig;
    g_bswMConfig.comConfig = &g_comConfig;
    g_bswMConfig.comMConfig = &g_commConfig;
    g_bswMConfig.canControllerId = CLUSTER_CAN_CONTROLLER;

    g_ecuMConfig.bswMConfig = &g_bswMConfig;
    return &g_ecuMConfig;
}
