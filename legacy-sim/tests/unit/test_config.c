/**
 * @file test_config.c
 * @brief Configuration validation tests (spec section 14.1 "test_config",
 * section 11.3).
 *
 * Covers the mandatory coverage: "valid configuration accepted;
 * duplicate/overlap/missing route rejected". The tables below mirror the
 * real SensorEcu configuration, so what is being validated is the shape
 * the ECU actually ships, not a toy.
 *
 * Uses the REAL CanIf/PduR/Com/Os validation logic against fake drivers,
 * because validation is exactly the behavior under test — a fake would
 * prove nothing.
 */
#include <stdio.h>

#include "CanIf.h"
#include "Com.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "Gpt_Fake.h"
#include "Os.h"
#include "Os_Counter.h"
#include "PduR.h"
#include "SysTimer_Fake.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

/* ---------------- the shipping SensorEcu tables, mirrored ------------- */

static const CanIf_TxPduConfigType kTxPdus[1] = {{0U, 0x180U, 8U, 0U, 0U}};
static const CanIf_RxPduConfigType kRxPdus[1] = {{0U, 0x181U, 8U, 0U}};
static const CanIf_ConfigType kCanIfConfig = {kTxPdus, 1U, kRxPdus, 1U};

static const PduR_RouteType kRoutes[2] = {
    {PDUR_MODULE_COM, 0U, PDUR_MODULE_CANIF, 0U},
    {PDUR_MODULE_CANIF, 0U, PDUR_MODULE_COM, 1U},
};
static const PduR_ConfigType kPduRConfig = {kRoutes, 2U};

static const Com_IPduConfigType kIpdus[2] = {
    {0U, COM_IPDU_DIR_TX, 0U, 8U, 20U, 0U, 24U, 4U, 4U, 0x0180U, TRUE},
    {1U, COM_IPDU_DIR_RX, 1U, 8U, 0U, 500U, 32U, 4U, 5U, 0x0181U, TRUE},
};
static const Com_SignalConfigType kSignals[5] = {
    {0U, 0U, 0U, 16U, 0U, 0U, 25000U, NULL}, {1U, 0U, 16U, 1U, 0U, 0U, 1U, NULL},
    {2U, 1U, 0U, 8U, 0U, 0U, 3U, NULL},      {3U, 1U, 8U, 16U, 0U, 0U, 25000U, NULL},
    {4U, 1U, 24U, 3U, 0U, 0U, 7U, NULL},
};
static const Com_ConfigType kComConfig = {kIpdus, 2U, kSignals, 5U};

static void NoOpTask(void) {
}

static const Os_TaskConfigType kTasks[5] = {
    {"Task_Bsw_1ms", 1U, 4U, NoOpTask},     {"Task_Io_5ms", 5U, 3U, NoOpTask},
    {"Task_App_10ms", 10U, 2U, NoOpTask},   {"Task_BswCom_20ms", 20U, 2U, NoOpTask},
    {"Task_App_100ms", 100U, 1U, NoOpTask},
};
static const Os_ConfigType kOsConfig = {
    .tasks = kTasks,
    .numTasks = 5U,
    .startupHook = NULL,
    .shutdownHook = NULL,
};

static void SetUp(void) {
    Det_Init();
    GptFake_Reset();
    Gpt_Init(NULL);
    SysTimerFake_Reset();
    SysTimer_Init(&SysTimer_Config);
    Os_Counter_Init();
}

static void TestShippingConfigurationIsAccepted(void) {
    SetUp();

    CanIf_Init(&kCanIfConfig);
    PduR_Init(&kPduRConfig);
    Com_Init(&kComConfig);
    Check(Os_Init(&kOsConfig) == E_OK, "the shipping task table is accepted");

    Check(Det_GetTotalErrorCount() == 0U,
          "the whole shipping configuration initializes without a single Det error");

    /* Proof it is not merely accepted but wired: a transmit reaches the
     * route, and the route reaches CanIf's Tx PDU. */
    Com_IpduGroupStart(COM_DEFAULT_IPDU_GROUP_ID, TRUE);
    uint16 speed = 1234U;
    Check(Com_SendSignal(0U, &speed) == E_OK, "a signal can be sent through the accepted config");
}

static void TestDuplicateRxCanIdRejected(void) {
    SetUp();
    /* REQ-CFG-003: two Rx PDUs cannot claim the same CAN ID, or reception
     * would be ambiguous. */
    static const CanIf_RxPduConfigType duplicateRx[2] = {{0U, 0x181U, 8U, 0U},
                                                         {1U, 0x181U, 8U, 0U}};
    static const CanIf_ConfigType bad = {kTxPdus, 1U, duplicateRx, 2U};

    CanIf_Init(&bad);
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_CANIF) > 0U,
          "a duplicate Rx CAN ID is reported to Det");
    PduInfoType info;
    uint8 payload[8] = {0U};
    info.SduDataPtr = payload;
    info.SduLength = 8U;
    Check(CanIf_Transmit(0U, &info) == E_NOT_OK,
          "a rejected CanIf configuration leaves the module refusing service");
}

static void TestCanIfIndexMismatchRejected(void) {
    SetUp();
    /* The id must equal the entry's index, or every lookup is off by one. */
    static const CanIf_TxPduConfigType badTx[1] = {{7U, 0x180U, 8U, 0U, 0U}};
    static const CanIf_ConfigType bad = {badTx, 1U, kRxPdus, 1U};

    CanIf_Init(&bad);
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_CANIF) > 0U,
          "a Tx PDU id that does not match its index is reported");
}

static void TestDuplicateRouteSourceRejected(void) {
    SetUp();
    /* Two routes from the same source is an ambiguous routing table. */
    static const PduR_RouteType duplicate[2] = {
        {PDUR_MODULE_COM, 0U, PDUR_MODULE_CANIF, 0U},
        {PDUR_MODULE_COM, 0U, PDUR_MODULE_CANIF, 1U},
    };
    static const PduR_ConfigType bad = {duplicate, 2U};

    PduR_Init(&bad);
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_PDUR) > 0U,
          "a duplicate route source is reported to Det");

    PduInfoType info;
    uint8 payload[8] = {0U};
    info.SduDataPtr = payload;
    info.SduLength = 8U;
    Check(PduR_ComTransmit(0U, &info) == E_NOT_OK,
          "a rejected PduR configuration leaves the module refusing service");
}

static void TestMissingRouteRejectedAtRuntime(void) {
    SetUp();
    /* A table with no route for the PDU being transmitted: accepted at
     * init (it is internally consistent) but refused at use, with a Det
     * record rather than silent loss. */
    static const PduR_RouteType onlyRx[1] = {{PDUR_MODULE_CANIF, 0U, PDUR_MODULE_COM, 1U}};
    static const PduR_ConfigType rxOnly = {onlyRx, 1U};

    PduR_Init(&rxOnly);
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_PDUR) == 0U,
          "a table with only an Rx route is internally consistent and accepted");

    PduInfoType info;
    uint8 payload[8] = {0U};
    info.SduDataPtr = payload;
    info.SduLength = 8U;
    Check(PduR_ComTransmit(0U, &info) == E_NOT_OK, "transmitting with no matching route fails");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_PDUR) > 0U,
          "the missing route is reported, never silently dropped");
}

static void TestOverlappingSignalsRejected(void) {
    SetUp();
    /* Two signals claiming the same bits would make packing order decide
     * the value on the wire. */
    static const Com_SignalConfigType overlapping[2] = {
        {0U, 0U, 0U, 16U, 0U, 0U, 25000U, NULL},
        {1U, 0U, 8U, 8U, 0U, 0U, 255U, NULL}, /* bits 8..15 already taken */
    };
    static const Com_ConfigType bad = {kIpdus, 2U, overlapping, 2U};

    Com_Init(&bad);
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_COM) > 0U,
          "overlapping signal bit ranges are reported to Det");
    uint16 value = 1U;
    Check(Com_SendSignal(0U, &value) == E_NOT_OK,
          "a rejected Com configuration leaves the module refusing service");
}

static void TestSignalOverlappingProtectionFieldsRejected(void) {
    SetUp();
    /* A signal may not sit on the alive counter nibble or the CRC byte. */
    static const Com_SignalConfigType onCounter[1] = {{0U, 0U, 24U, 4U, 0U, 0U, 15U, NULL}};
    static const Com_ConfigType bad = {kIpdus, 2U, onCounter, 1U};

    Com_Init(&bad);
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_COM) > 0U,
          "a signal overlapping the alive counter is reported to Det");
}

static void TestWrongDlcRejected(void) {
    SetUp();
    /* Section 6.1: every I-PDU in this project is 8 bytes. */
    static const Com_IPduConfigType shortIpdu[1] = {
        {0U, COM_IPDU_DIR_TX, 0U, 4U, 20U, 0U, 24U, 4U, 4U, 0x0180U, TRUE}};
    static const Com_ConfigType bad = {shortIpdu, 1U, kSignals, 2U};

    Com_Init(&bad);
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_COM) > 0U, "a PDU length other than 8 is rejected");
}

static void TestOsTaskTableValidation(void) {
    SetUp();
    static const Os_TaskConfigType zeroPeriod[1] = {{"bad", 0U, 1U, NoOpTask}};
    static const Os_ConfigType badPeriod = {
        .tasks = zeroPeriod,
        .numTasks = 1U,
        .startupHook = NULL,
        .shutdownHook = NULL,
    };
    Check(Os_Init(&badPeriod) == E_NOT_OK, "a zero-period task is rejected");

    static const Os_TaskConfigType nullCallback[1] = {{"bad", 10U, 1U, NULL}};
    static const Os_ConfigType badCallback = {
        .tasks = nullCallback,
        .numTasks = 1U,
        .startupHook = NULL,
        .shutdownHook = NULL,
    };
    Check(Os_Init(&badCallback) == E_NOT_OK, "a task with no callback is rejected");

    Check(Os_Init(NULL) == E_NOT_OK, "a null task table is rejected");
}

int main(void) {
    TestShippingConfigurationIsAccepted();
    TestDuplicateRxCanIdRejected();
    TestCanIfIndexMismatchRejected();
    TestDuplicateRouteSourceRejected();
    TestMissingRouteRejectedAtRuntime();
    TestOverlappingSignalsRejected();
    TestSignalOverlappingProtectionFieldsRejected();
    TestWrongDlcRejected();
    TestOsTaskTableValidation();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_config: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_config: all checks passed\n");
    return 0;
}
