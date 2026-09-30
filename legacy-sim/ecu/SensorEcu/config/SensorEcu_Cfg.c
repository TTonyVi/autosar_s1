/**
 * @file SensorEcu_Cfg.c
 * @brief The SensorEcu's static configuration and task bodies.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 6.2, 6.3, 7.5, 8.2, 11.
 *
 * This translation unit is the ONLY place on this ECU that knows both a
 * socket path and a CAN ID. Everything above MCAL receives them as opaque
 * configuration (section 15.4).
 */
#include "SensorEcu_Cfg.h"

#include <stddef.h>

#include "BswM.h"
#include "Com.h"
#include "Dem.h"
#include "Det.h"
#include "Logger.h"
#include "Rte_SensorEcu.h"
#include "Rte_SensorEcu_Cfg.h"
#include "VehicleStateSwc.h"

/* ------------------------- MCAL: base drivers ------------------------ */

static const Mcu_ConfigType g_mcuConfig = {.reserved = 0U};
static const Port_ConfigType g_portConfig = {.reserved = 0U};
static const Gpt_ChannelConfigType g_gptChannels[1] = {{.channelId = GPT_CHANNEL_TIME_BASE}};
static const Gpt_ConfigType g_gptConfig = {.channels = g_gptChannels, .numChannels = 1U};

/* ------------------- MCAL: the virtual sensor line -------------------
 * The SensorEcu is the CONSUMER, so it binds (section 15.5.2). The path
 * is a post-build-like value, patched in by SensorEcu_BuildConfig. */
#define SENSOR_LINK_ID 0U

static VirtualHw_LinkConfigType g_vhwLinks[1] = {{
    .linkId = SENSOR_LINK_ID,
    .direction = VIRTUALHW_DIR_RX,
    .socketPath = NULL, /* patched from the command line */
    .maxRxPerActivation = 8U,
    .stalenessTimeoutMs = IOHWAB_INPUT_TIMEOUT_MS,
}};
static const VirtualHw_ConfigType g_vhwConfig = {.links = g_vhwLinks, .numLinks = 1U};

/* --------------------- MCAL: the input drivers ----------------------- */

static const Adc_GroupConfigType g_adcGroups[1] = {{
    .groupId = (Adc_GroupType)IOHWAB_ADC_GROUP_WHEEL_SPEED,
    .channelId = 0U, /* wire key "ADC0" */
    .resolutionBits = 12U,
    .virtualHwLinkId = SENSOR_LINK_ID,
    .plausibilityMinCounts = (uint16)IOHWAB_SPEED_PLAUSIBILITY_MIN_COUNTS,
    .plausibilityMaxCounts = (uint16)IOHWAB_SPEED_PLAUSIBILITY_MAX_COUNTS,
}};
static const Adc_ConfigType g_adcConfig = {.groups = g_adcGroups, .numGroups = 1U};

static const Dio_ChannelConfigType g_dioChannels[1] = {{
    .channelId = (Dio_ChannelType)IOHWAB_DIO_CHANNEL_IGNITION, /* wire key "DIO0" */
    .virtualHwLinkId = SENSOR_LINK_ID,
    .defaultLevel = STD_LOW, /* ignition reads off until a datagram says otherwise */
}};
static const Dio_ConfigType g_dioConfig = {.channels = g_dioChannels, .numChannels = 1U};

/* ------------------------- MCAL: CAN driver -------------------------- */
#define SENSOR_CAN_CONTROLLER 0U

static Can_ControllerConfigType g_canControllers[1] = {{
    .controllerId = SENSOR_CAN_CONTROLLER,
    .interfaceName = NULL, /* patched from the command line */
    .nonBlocking = TRUE,
}};
static const Can_ConfigType g_canConfig = {.controllers = g_canControllers, .numControllers = 1U};

/* --------------------- CanIf: section 6.2 and 7.5 -------------------- */

static const CanIf_TxPduConfigType g_canIfTxPdus[1] = {{
    .canIfTxPduId = 0U,
    .canId = 0x180U, /* VehicleStatus */
    .dlc = 8U,
    .hth = 0U,
    .controllerId = SENSOR_CAN_CONTROLLER,
}};
static const CanIf_RxPduConfigType g_canIfRxPdus[1] = {{
    .canIfRxPduId = 0U,
    .canId = 0x181U, /* ClusterStatus */
    .dlc = 8U,
    .controllerId = SENSOR_CAN_CONTROLLER,
}};
static const CanIf_ConfigType g_canIfConfig = {
    .txPdus = g_canIfTxPdus,
    .numTxPdus = 1U,
    .rxPdus = g_canIfRxPdus,
    .numRxPdus = 1U,
};

/* --------------------------- PduR: section 7.5 ----------------------- */

static const PduR_RouteType g_pduRRoutes[2] = {
    /* Tx: COM VehicleStatus -> CanIf Tx PDU 0 -> CAN 0x180 */
    {PDUR_MODULE_COM, 0U, PDUR_MODULE_CANIF, 0U},
    /* Rx: CAN 0x181 -> CanIf Rx PDU 0 -> COM ClusterStatus (I-PDU 1) */
    {PDUR_MODULE_CANIF, 0U, PDUR_MODULE_COM, 1U},
};
static const PduR_ConfigType g_pduRConfig = {.routes = g_pduRRoutes, .numRoutes = 2U};

/* ---------------- COM: sections 6.2, 6.3, 6.4 and 6.5 ----------------
 * Layout identical to the one proven byte-for-byte against the section 6.5
 * golden vectors in test_com_pack. */

static const Com_IPduConfigType g_comIpdus[RTE_NUM_IPDUS] = {
    {
        .ipduId = RTE_IPDU_TX_VEHICLE_STATUS,
        .direction = COM_IPDU_DIR_TX,
        .pduRPduId = RTE_IPDU_TX_VEHICLE_STATUS,
        .length = 8U,
        .periodMs = 20U, /* section 6.2 */
        .timeoutMs = 0U,
        .counterBitPosition = 24U, /* byte 3, bits 0..3 */
        .counterBitLength = 4U,
        .crcByteIndex = 4U,
        .protectionDataId = 0x0180U,
        .protectionEnabled = TRUE,
    },
    {
        .ipduId = RTE_IPDU_RX_CLUSTER_STATUS,
        .direction = COM_IPDU_DIR_RX,
        .pduRPduId = RTE_IPDU_RX_CLUSTER_STATUS,
        .length = 8U,
        .periodMs = 0U,
        .timeoutMs = 500U,         /* section 6.2 deadline for ClusterStatus */
        .counterBitPosition = 32U, /* byte 4, bits 0..3 */
        .counterBitLength = 4U,
        .crcByteIndex = 5U,
        .protectionDataId = 0x0181U,
        .protectionEnabled = TRUE,
    },
};

static const Com_SignalConfigType g_comSignals[RTE_NUM_SIGNALS] = {
    {RTE_SIGNAL_TX_VEHICLE_SPEED_RAW, RTE_IPDU_TX_VEHICLE_STATUS, 0U, 16U, 0U, 0U, 25000U, NULL},
    {RTE_SIGNAL_TX_IGNITION_ON, RTE_IPDU_TX_VEHICLE_STATUS, 16U, 1U, 0U, 0U, 1U, NULL},
    {RTE_SIGNAL_RX_CLUSTER_STATE, RTE_IPDU_RX_CLUSTER_STATUS, 0U, 8U, 0U, 0U, 3U,
     Rte_COMCbk_ClusterState},
    {RTE_SIGNAL_RX_DISPLAYED_SPEED_RAW, RTE_IPDU_RX_CLUSTER_STATUS, 8U, 16U, 0U, 0U, 25000U,
     Rte_COMCbk_DisplayedSpeedRaw},
    {RTE_SIGNAL_RX_STATUS_FLAGS, RTE_IPDU_RX_CLUSTER_STATUS, 24U, 3U, 0U, 0U, 7U,
     Rte_COMCbk_StatusFlags},
};

static const Com_ConfigType g_comConfig = {
    .ipdus = g_comIpdus,
    .numIpdus = (uint8)RTE_NUM_IPDUS,
    .signals = g_comSignals,
    .numSignals = (uint8)RTE_NUM_SIGNALS,
};

/* ------------------------- Os: section 8.2 --------------------------- */

/* Vai trò: thân task ưu tiên cao nhất, chu kỳ 1ms — chỉ đọc/ghi socket
 * CAN (không bao giờ log ở đây, section 13.4). */
static void Task_Bsw_1ms(void) {
    /* Section 13.4: the 1 ms task never logs normal activity. */
    Can_MainFunction_Read();
    Can_MainFunction_Write();
}

/* Vai trò: thân task chu kỳ 5ms — kéo Adc/Dio đọc virtual hw link rồi
 * chạy IoHwAb_MainFunction (scale + đánh giá validity). */
static void Task_Io_5ms(void) {
    Adc_MainFunction_Read();
    Dio_MainFunction_Read();
    IoHwAb_MainFunction();
}

/* Vai trò: thân task chu kỳ 10ms — chạy runnable ứng dụng qua RTE. */
static void Task_App_10ms(void) {
    Rte_Task_10ms();
}

/* Vai trò: thân task chu kỳ 20ms — chạy Com Tx/Rx (đóng gói/gửi, kiểm tra
 * timeout). */
static void Task_BswCom_20ms(void) {
    Com_MainFunctionTx();
    Com_MainFunctionRx();
}

/** Vai trò: quét toàn bộ event Dem mỗi chu kỳ, chỉ in log khi trạng thái
 * MỘT event nào đó VỪA THAY ĐỔI (không in lặp lại mỗi lần cùng trạng
 * thái) — giữ khối lượng log ở mức chấp nhận được (section 15.8).
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

/** Vai trò: in 2 dòng log tổng kết mỗi giây (không phải mỗi lần chạy —
 * tick counter chặn lại) — 1 dòng trạng thái chung, 1 dòng "[Layers]" liệt
 * kê counter riêng của từng tầng (Rte/Com/PduR/CanIf/Adc/IoHwAb) để test
 * IT-04 xác nhận dữ liệu thật sự đi qua từng lớp, không chỉ nhìn kết quả
 * cuối. Chỉ phục vụ quan sát/test, không module nào đọc lại các số này.
 * Bounded periodic status line (section 13.4): one line per second, not
 *  one per activation. */
static void LogStatus(void) {
    static uint8 tick;
    tick++;
    if (tick < 10U) {
        return;
    }
    tick = 0U;

    Dem_EventRecordType record;
    Dem_EventStatusType clusterTimeout = DEM_EVENT_STATUS_PASSED;
    if (Dem_GetEventRecord(DEM_EVENT_CLUSTER_STATUS_TIMEOUT, &record) == E_OK) {
        clusterTimeout = record.currentStatus;
    }

    Logger_Log(LOGGER_LEVEL_INFO, "SensorEcu",
               "remote_state=%u remote_status=%u invalid_inputs=%u tx=%u cluster_timeout=%u",
               (unsigned)VehicleStateSwc_GetLastRemoteClusterState(),
               (unsigned)VehicleStateSwc_GetLastRemoteStatus(),
               (unsigned)VehicleStateSwc_GetInvalidInputCount(),
               (unsigned)Com_GetIpduTxCount((Com_IPduIdType)RTE_IPDU_TX_VEHICLE_STATUS),
               (unsigned)clusterTimeout);

    /* IT-04 "Layered Tx/Rx evidence": one line naming every layer's own
     * counter for both the output (Tx) and input paths, so an
     * integration test can assert each layer actually moved, not just
     * that the end result looks right. Test instrumentation only — none
     * of these values is read back by any module's own logic. */
    Logger_Log(LOGGER_LEVEL_INFO, "Layers",
               "rte_tx=%u com_tx=%u pdur_tx_fwd=%u canif_tx_conf=%u | "
               "adc_reads=%u iohwab_valid=%u rte_rx=%u",
               (unsigned)Rte_GetTxWriteCount(),
               (unsigned)Com_GetIpduTxCount((Com_IPduIdType)RTE_IPDU_TX_VEHICLE_STATUS),
               (unsigned)PduR_GetComTransmitForwardCount(),
               (unsigned)CanIf_GetTxConfirmationCount(),
               (unsigned)Adc_GetGroupReadCount((Adc_GroupType)IOHWAB_ADC_GROUP_WHEEL_SPEED),
               (unsigned)IoHwAb_GetValidSampleCount(), (unsigned)Rte_GetRxUpdateCount());
}

/* Vai trò: thân task ưu tiên thấp nhất, chu kỳ 100ms — chạy runnable
 * 100ms qua RTE, tổng hợp Dem, rồi in 2 dòng log định kỳ. */
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
    {.canControllerId = SENSOR_CAN_CONTROLLER, .networkHandle = COMM_CHANNEL_CAN},
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

static const BswM_ConfigType g_bswMConfig = {
    .virtualHwConfig = &g_vhwConfig,
    .adcConfig = &g_adcConfig,
    .dioConfig = &g_dioConfig,
    .displayConfig = NULL,
    .canConfig = &g_canConfig,
    .ioHwAbConfig = &IoHwAb_ConfigSensor,
    .canIfConfig = &g_canIfConfig,
    .canSMConfig = &g_canSMConfig,
    .pduRConfig = &g_pduRConfig,
    .comConfig = &g_comConfig,
    .comMConfig = &g_commConfig,
    .canControllerId = SENSOR_CAN_CONTROLLER,
};

static void SensorEcu_StartupHook(void) {
    EcuM_StartupTwo();
    if (BswM_Init(&g_bswMConfig) != E_OK) {
        Logger_Log(LOGGER_LEVEL_ERROR, "SensorEcu", "BswM_Init failed");
    }
}

static const Os_ConfigType g_osConfig = {
    .tasks = g_osTasks,
    .numTasks = 5U,
    .startupHook = SensorEcu_StartupHook,
    .shutdownHook = NULL,
};

/* --------------------------- assembly -------------------------------- */

static EcuM_ConfigType g_ecuMConfig;

/* Vai trò: LẮP RÁP toàn bộ EcuM_ConfigType cho SensorEcu — vá 2 giá trị
 * "post-build" duy nhất (đường socket + tên interface CAN, đến từ dòng
 * lệnh) vào các bảng const tĩnh đã khai báo sẵn ở trên, rồi trả về con
 * trỏ cho main() đưa vào EcuM_Init. Đây là nơi DUY NHẤT trên ECU này biết
 * cả đường dẫn socket lẫn CAN ID cùng lúc. */
const EcuM_ConfigType *SensorEcu_BuildConfig(const EcuM_PostBuildType *postBuild) {
    if ((postBuild == NULL) || (postBuild->canInterface == NULL) ||
        (postBuild->hwLinkPath == NULL)) {
        return NULL;
    }

    /* Post-build-like values reach MCAL configuration and stop there. */
    g_vhwLinks[0].socketPath = postBuild->hwLinkPath;
    g_canControllers[0].interfaceName = postBuild->canInterface;

    g_ecuMConfig.ecuName = "SensorEcu";
    g_ecuMConfig.logLevel = postBuild->logLevel;
    g_ecuMConfig.durationSeconds = postBuild->durationSeconds;
    g_ecuMConfig.mcuConfig = &g_mcuConfig;
    g_ecuMConfig.portConfig = &g_portConfig;
    g_ecuMConfig.sysTimerConfig = &SysTimer_Config;
    g_ecuMConfig.gptConfig = &g_gptConfig;
    g_ecuMConfig.virtualHwConfig = &g_vhwConfig;
    g_ecuMConfig.adcConfig = &g_adcConfig;
    g_ecuMConfig.dioConfig = &g_dioConfig;
    g_ecuMConfig.displayConfig = NULL; /* the sensor ECU owns no panel */
    g_ecuMConfig.canConfig = &g_canConfig;
    g_ecuMConfig.ioHwAbConfig = &IoHwAb_ConfigSensor;
    g_ecuMConfig.canIfConfig = &g_canIfConfig;
    g_ecuMConfig.pduRConfig = &g_pduRConfig;
    g_ecuMConfig.comConfig = &g_comConfig;
    g_ecuMConfig.osConfig = &g_osConfig;
    g_ecuMConfig.bswMConfig = &g_bswMConfig;
    g_ecuMConfig.canControllerId = SENSOR_CAN_CONTROLLER;
    return &g_ecuMConfig;
}
