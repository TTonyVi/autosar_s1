/**
 * @file test_com_pack.c
 * @brief Phase 4 unit tests for Com (bsw/communication/Com).
 *
 * Covers section 14.1 ("test_com_pack") and the Phase 4 exit criteria
 * (section 16): "raw CAN injection reaches a mock RTE callback; COM Tx
 * produces exact expected bytes." Uses the REAL Com.c and ComProtection.c
 * against a FAKE PduR (tests/fixtures/PduR_Fake.c, to capture exactly
 * what bytes were requested to send) and a FAKE Gpt
 * (tests/fixtures/Gpt_Fake.c, for deterministic control over cyclic Tx
 * timing and Rx deadline supervision) -- same seam pattern as every prior
 * phase's tests.
 *
 * The config below mirrors sections 6.2-6.4's real VehicleStatus (TX)
 * and ClusterStatus (RX) layouts exactly, so the golden vectors of
 * section 6.5 can be asserted byte-for-byte.
 */
#include <stdio.h>
#include <string.h>

#include "Com.h"
#include "ComProtection.h"
#include "Det.h"
#include "Gpt_Fake.h"
#include "PduR_Fake.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

#define SIG_VEHICLE_SPEED_RAW 0U
#define SIG_IGNITION_ON 1U
#define SIG_CLUSTER_STATE 2U
#define SIG_DISPLAYED_SPEED_RAW 3U
#define SIG_STATUS_FLAGS 4U

#define IPDU_VEHICLE_STATUS 0U /* TX, mirrors section 6.3 */
#define IPDU_CLUSTER_STATUS 1U /* RX, mirrors section 6.4 */

static uint32 g_mockNotifyCount;

static void MockRteNotify(void) {
    g_mockNotifyCount++;
}

static const Com_IPduConfigType g_ipdus[2] = {
    {IPDU_VEHICLE_STATUS, COM_IPDU_DIR_TX, 0U, 8U, 20U, 0U, 24U, 4U, 4U, 0x0180U, TRUE},
    {IPDU_CLUSTER_STATUS, COM_IPDU_DIR_RX, 1U, 8U, 0U, 500U, 32U, 4U, 5U, 0x0181U, TRUE},
};

static const Com_SignalConfigType g_signals[5] = {
    {SIG_VEHICLE_SPEED_RAW, IPDU_VEHICLE_STATUS, 0U, 16U, 0U, 0U, 25000U, NULL},
    {SIG_IGNITION_ON, IPDU_VEHICLE_STATUS, 16U, 1U, 0U, 0U, 1U, NULL},
    {SIG_CLUSTER_STATE, IPDU_CLUSTER_STATUS, 0U, 8U, 0U, 0U, 3U, MockRteNotify},
    {SIG_DISPLAYED_SPEED_RAW, IPDU_CLUSTER_STATUS, 8U, 16U, 0U, 0U, 25000U, MockRteNotify},
    {SIG_STATUS_FLAGS, IPDU_CLUSTER_STATUS, 24U, 3U, 0U, 0U, 7U, MockRteNotify},
};

static const Com_ConfigType g_config = {g_ipdus, 2U, g_signals, 5U};

static void SetUp(void) {
    Det_Init();
    GptFake_Reset();
    PduRFake_Reset();
    g_mockNotifyCount = 0U;
    Com_Init(&g_config);
    Com_IpduGroupStart(COM_DEFAULT_IPDU_GROUP_ID, TRUE);
}

/* Builds a valid ClusterStatus-shaped frame using the already-tested real
 * ComProtection, so tests can construct precise wire content instead of
 * hand-computing CRC bytes by hand for every scenario. */
static void BuildClusterStatusFrame(uint8 clusterState, uint16 displayedSpeed, uint8 flags,
                                    uint8 counter, uint8 frame[8]) {
    (void)memset(frame, 0, 8U);
    frame[0] = clusterState;
    frame[1] = (uint8)(displayedSpeed & 0xFFU);
    frame[2] = (uint8)(displayedSpeed >> 8U);
    frame[3] = (uint8)(flags & 0x07U);
    frame[4] = (uint8)(counter & 0x0FU);
    frame[5] = ComProtection_ComputeCrc8(0x0181U, frame, 5U);
}

static void TestTxProducesExactGoldenBytes(void) {
    SetUp();
    uint16 speed = 1234U; /* 0x04D2 */
    uint16 ignition = 1U;
    Check(Com_SendSignal(SIG_VEHICLE_SPEED_RAW, &speed) == E_OK, "speed accepted");
    Check(Com_SendSignal(SIG_IGNITION_ON, &ignition) == E_OK, "ignition accepted");

    Com_MainFunctionTx(); /* group start with initialize=TRUE made this due immediately */

    Check(PduRFake_GetComTransmitCount() == 1U, "exactly one transmit was requested");
    uint8 bytes[8];
    uint8 len = 0U;
    PduIdType id = 0xFFFFU;
    Check(PduRFake_GetLastComTransmit(&id, bytes, sizeof(bytes), &len), "a transmit was recorded");
    Check(id == 0U, "the configured pduRPduId is used");
    Check(len == 8U, "length is 8");
    uint8 expected[8] = {0xD2U, 0x04U, 0x01U, 0x00U, 0x19U, 0x00U, 0x00U, 0x00U};
    Check(memcmp(bytes, expected, 8U) == 0,
          "COM Tx produces the exact section 6.5 golden-vector bytes for VehicleStatus");
}

static void TestAliveCounterAdvancesOnlyOnSuccessfulRequest(void) {
    SetUp();
    uint16 speed = 0U;
    uint16 ignition = 0U;
    (void)Com_SendSignal(SIG_VEHICLE_SPEED_RAW, &speed);
    (void)Com_SendSignal(SIG_IGNITION_ON, &ignition);

    PduRFake_SetComTransmitResult(E_NOT_OK);
    Com_MainFunctionTx();
    Check(Com_GetIpduTxFailureCount(IPDU_VEHICLE_STATUS) == 1U, "the failed request is counted");

    GptFake_AdvanceMs(20U);
    PduRFake_SetComTransmitResult(E_OK);
    Com_MainFunctionTx();
    uint8 bytes[8];
    uint8 len = 0U;
    PduIdType id;
    (void)PduRFake_GetLastComTransmit(&id, bytes, sizeof(bytes), &len);
    Check(bytes[3] == 0U,
          "the alive counter is still 0 in the first successful frame -- it never advanced "
          "after the earlier failed request");
    Check(Com_GetIpduTxCount(IPDU_VEHICLE_STATUS) == 1U, "exactly one successful transmit so far");

    GptFake_AdvanceMs(20U);
    Com_MainFunctionTx();
    (void)PduRFake_GetLastComTransmit(&id, bytes, sizeof(bytes), &len);
    Check(bytes[3] == 1U, "the alive counter advances to 1 after the second successful transmit");
}

static void TestPeriodGating(void) {
    SetUp();
    Com_MainFunctionTx(); /* consume the immediate-due send from group start */
    uint32 countAfterFirst = Com_GetIpduTxCount(IPDU_VEHICLE_STATUS);

    Com_MainFunctionTx(); /* called again at the same simulated time -- not due yet */
    Check(Com_GetIpduTxCount(IPDU_VEHICLE_STATUS) == countAfterFirst,
          "a second call before the 20ms period elapses sends nothing new");

    GptFake_AdvanceMs(20U);
    Com_MainFunctionTx();
    Check(Com_GetIpduTxCount(IPDU_VEHICLE_STATUS) == countAfterFirst + 1U,
          "sends again once its configured period elapses");
}

static void TestGroupMustBeStartedBeforeSending(void) {
    Det_Init();
    GptFake_Reset();
    PduRFake_Reset();
    Com_Init(&g_config); /* deliberately no Com_IpduGroupStart call */

    uint16 speed = 100U;
    uint16 ignition = 0U;
    (void)Com_SendSignal(SIG_VEHICLE_SPEED_RAW, &speed);
    (void)Com_SendSignal(SIG_IGNITION_ON, &ignition);
    Com_MainFunctionTx();
    Check(PduRFake_GetComTransmitCount() == 0U,
          "nothing is sent before Com_IpduGroupStart is called");
}

static void TestSendSignalOutOfRangeRejected(void) {
    SetUp();
    uint16 tooFast = 30000U; /* > 25000 max, section 4.3 */
    Check(Com_SendSignal(SIG_VEHICLE_SPEED_RAW, &tooFast) == E_NOT_OK,
          "Com_SendSignal rejects an out-of-range value");
}

static void TestRxRawInjectionReachesMockRteCallback(void) {
    SetUp();
    uint8 frame[8] = {0x01U, 0xD2U, 0x04U, 0x00U, 0x00U, 0xB0U, 0x00U, 0x00U}; /* golden vector 2 */
    PduInfoType info;
    info.SduDataPtr = frame;
    info.SduLength = 8U;

    Check(g_mockNotifyCount == 0U, "no notifications yet");
    Com_RxIndication(IPDU_CLUSTER_STATUS, &info);
    Check(Com_GetIpduRxValidCount(IPDU_CLUSTER_STATUS) == 1U,
          "the raw CAN frame is accepted as a valid PDU");
    Check(g_mockNotifyCount == 3U,
          "raw CAN injection reaches the mock RTE callback, once per signal owned by the PDU");

    uint16 clusterState = 99U;
    uint16 displayedSpeed = 99U;
    uint16 flags = 99U;
    (void)Com_ReceiveSignal(SIG_CLUSTER_STATE, &clusterState);
    (void)Com_ReceiveSignal(SIG_DISPLAYED_SPEED_RAW, &displayedSpeed);
    (void)Com_ReceiveSignal(SIG_STATUS_FLAGS, &flags);
    Check(clusterState == 1U, "ClusterState decoded correctly");
    Check(displayedSpeed == 1234U, "DisplayedSpeedRaw decoded correctly");
    Check(flags == 0U, "StatusFlags decoded correctly");
}

static void TestRxWrongCrcRejectedWithoutUpdatingSignalsOrNotifying(void) {
    SetUp();
    uint8 good[8] = {0x01U, 0xD2U, 0x04U, 0x00U, 0x00U, 0xB0U, 0x00U, 0x00U};
    PduInfoType goodInfo;
    goodInfo.SduDataPtr = good;
    goodInfo.SduLength = 8U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &goodInfo);
    uint32 notifyBefore = g_mockNotifyCount;

    uint8 bad[8] = {0x02U, 0x00U, 0x00U, 0x00U, 0x01U, 0xFFU, 0x00U, 0x00U}; /* wrong CRC byte */
    PduInfoType badInfo;
    badInfo.SduDataPtr = bad;
    badInfo.SduLength = 8U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &badInfo);

    Check(Com_GetIpduRxCrcErrorCount(IPDU_CLUSTER_STATUS) == 1U, "the CRC failure is counted");
    Check(g_mockNotifyCount == notifyBefore, "no notification fires for a CRC-invalid frame");
    uint16 clusterState = 0U;
    (void)Com_ReceiveSignal(SIG_CLUSTER_STATE, &clusterState);
    Check(clusterState == 1U,
          "the previous valid value is untouched by the CRC-invalid frame (REQ-ASW-005)");
}

static void TestRxWrongDlcRejectedBeforeTouchingPayload(void) {
    SetUp();
    uint8 shortFrame[4] = {0U, 0U, 0U, 0U};
    PduInfoType info;
    info.SduDataPtr = shortFrame;
    info.SduLength = 4U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &info);
    Check(Com_GetIpduRxDlcErrorCount(IPDU_CLUSTER_STATUS) == 1U,
          "a wrong DLC is rejected before ever reading a signal (REQ-COM-002)");
}

static void TestRxSkippedCounterRejectedThenRecoversOnCorrectNext(void) {
    SetUp();
    uint8 frame0[8];
    BuildClusterStatusFrame(1U, 1234U, 0U, 0U, frame0);
    PduInfoType info0;
    info0.SduDataPtr = frame0;
    info0.SduLength = 8U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &info0);

    uint8 frameSkip[8];
    BuildClusterStatusFrame(2U, 2000U, 0U, 2U, frameSkip); /* expected next is 1, not 2 */
    PduInfoType infoSkip;
    infoSkip.SduDataPtr = frameSkip;
    infoSkip.SduLength = 8U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &infoSkip);
    Check(Com_GetIpduRxCounterErrorCount(IPDU_CLUSTER_STATUS) == 1U,
          "a skipped alive counter is rejected (REQ-COM-003)");

    uint16 clusterState = 0U;
    (void)Com_ReceiveSignal(SIG_CLUSTER_STATE, &clusterState);
    Check(clusterState == 1U, "the previous value is untouched by the rejected frame");

    uint8 frame1[8];
    BuildClusterStatusFrame(2U, 2000U, 0U, 1U, frame1); /* the correct next counter */
    PduInfoType info1;
    info1.SduDataPtr = frame1;
    info1.SduLength = 8U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &info1);
    Check(Com_GetIpduRxValidCount(IPDU_CLUSTER_STATUS) == 2U,
          "the correctly-sequenced frame after the rejected one is accepted");
}

static void TestRxSignalOutOfRangeInvalidatesWholePdu(void) {
    SetUp();
    uint8 frame[8];
    BuildClusterStatusFrame(1U, 30000U /* > 25000 max */, 0U, 0U, frame);
    PduInfoType info;
    info.SduDataPtr = frame;
    info.SduLength = 8U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &info);
    Check(Com_GetIpduRxSignalRangeErrorCount(IPDU_CLUSTER_STATUS) == 1U,
          "an out-of-range signal value invalidates the whole PDU (REQ-COM-004)");
    Check(g_mockNotifyCount == 0U, "no notification for a rejected PDU");
}

static void TestRxTimeoutSupervision(void) {
    SetUp();
    uint8 frame[8];
    BuildClusterStatusFrame(1U, 1234U, 0U, 0U, frame);
    PduInfoType info;
    info.SduDataPtr = frame;
    info.SduLength = 8U;
    Com_RxIndication(IPDU_CLUSTER_STATUS, &info);

    boolean timedOut = TRUE;
    Check(Com_GetIpduRxTimeoutStatus(IPDU_CLUSTER_STATUS, &timedOut),
          "timeout status is retrievable after a valid receive");
    Check(!timedOut, "not timed out immediately after a fresh valid receive");

    GptFake_AdvanceMs(600U); /* > the configured 500ms timeout */
    Com_MainFunctionRx();
    Check(Com_GetIpduRxTimeoutStatus(IPDU_CLUSTER_STATUS, &timedOut), "status still retrievable");
    Check(timedOut,
          "REQ-COM-005: a timeout is based on monotonic time since the last VALID reception");
}

int main(void) {
    TestTxProducesExactGoldenBytes();
    TestAliveCounterAdvancesOnlyOnSuccessfulRequest();
    TestPeriodGating();
    TestGroupMustBeStartedBeforeSending();
    TestSendSignalOutOfRangeRejected();
    TestRxRawInjectionReachesMockRteCallback();
    TestRxWrongCrcRejectedWithoutUpdatingSignalsOrNotifying();
    TestRxWrongDlcRejectedBeforeTouchingPayload();
    TestRxSkippedCounterRejectedThenRecoversOnCorrectNext();
    TestRxSignalOutOfRangeInvalidatesWholePdu();
    TestRxTimeoutSupervision();

    if (g_failures == 0) {
        (void)printf("test_com_pack: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_com_pack: %d check(s) failed\n", g_failures);
    return 1;
}
