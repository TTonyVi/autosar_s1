/**
 * @file test_rte_cluster.c
 * @brief Unit tests for the ClusterEcu RTE (rte/generated/ClusterEcu).
 *
 * Covers spec section 14.1 row "test_rte_cluster": atomic Rx update, an
 * invalid frame does not overwrite, write mapping.
 *
 * Exercises the REAL RTE and the REAL ClusterControlSwc against a FAKE Com
 * and a FAKE IoHwAb.
 */
#include <stdio.h>

#include "ClusterControlSwc.h"
#include "Com_Fake.h"
#include "Det.h"
#include "IoHwAb_Fake.h"
#include "Rte_ClusterControlSwc.h"
#include "Rte_ClusterEcu.h"
#include "Rte_ClusterEcu_Cfg.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void SetUp(void) {
    Det_Init();
    ComFake_ResetSignals();
    IoHwAbFake_Reset();
    (void)Rte_Start();
}

/** Delivers one complete VehicleStatus reception, both signals. */
static void DeliverVehicleStatus(uint16 speed, uint16 ignition) {
    ComFake_SetReceivedSignal((Com_SignalIdType)RTE_SIGNAL_RX_VEHICLE_SPEED_RAW, speed);
    ComFake_SetReceivedSignal((Com_SignalIdType)RTE_SIGNAL_RX_IGNITION_ON, ignition);
    Rte_COMCbk_VehicleSpeedRaw();
    Rte_COMCbk_IgnitionOn();
}

static void TestWriteMapsOntoComSignals(void) {
    SetUp();

    Check(Rte_Write_PpClusterStatus_ClusterState(CLUSTER_STATE_OVERSPEED) == RTE_E_OK,
          "a state write succeeds");
    Check(Rte_Write_PpClusterStatus_DisplayedSpeedRaw(11111U) == RTE_E_OK,
          "a speed write succeeds");
    Check(Rte_Write_PpClusterStatus_StatusFlags(CLUSTER_FLAG_OVERSPEED) == RTE_E_OK,
          "a flags write succeeds");

    uint16 sent = 0U;
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_CLUSTER_STATE, &sent);
    Check(sent == (uint16)CLUSTER_STATE_OVERSPEED, "the state maps to its COM signal");
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_DISPLAYED_SPEED_RAW, &sent);
    Check(sent == 11111U, "the displayed speed maps to its COM signal");
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_STATUS_FLAGS, &sent);
    Check(sent == (uint16)CLUSTER_FLAG_OVERSPEED, "the flags map to their COM signal");
}

static void TestRxUpdateIsAtomicForTheSwc(void) {
    SetUp();

    /* REQ-RTE-003: both elements of one I-PDU become visible together from
     * the single ECU thread's point of view — COM delivers both
     * notifications before any runnable can observe either. */
    DeliverVehicleStatus(8000U, 1U);

    VehicleSpeedRawType speed = 0U;
    IgnitionOnType ignition = FALSE;
    Check(Rte_Read_RpVehicleState_VehicleSpeedRaw(&speed) == RTE_E_OK, "the speed read is OK");
    Check(Rte_Read_RpVehicleState_IgnitionOn(&ignition) == RTE_E_OK, "the ignition read is OK");
    Check(speed == 8000U, "the received speed is visible");
    Check(ignition == TRUE, "the received ignition is visible");
    Check(Rte_GetRxUpdateCount() == 2U, "both elements of the PDU updated the buffers");
}

static void TestInvalidFrameDoesNotOverwrite(void) {
    SetUp();

    DeliverVehicleStatus(8000U, 1U);
    Rte_Task_10ms(); /* baseline the counters */

    /* COM refuses to update signals for a frame that failed CRC or the
     * alive counter (REQ-COM-003/REQ-RTE-005). The RTE must therefore
     * still hold the previous values, and must say so. */
    ComFake_SetRxCounterErrorCount((Com_IPduIdType)RTE_IPDU_RX_VEHICLE_STATUS, 1U);
    Rte_Task_10ms();

    VehicleSpeedRawType speed = 0U;
    Check(Rte_Read_RpVehicleState_VehicleSpeedRaw(&speed) == RTE_E_INVALID,
          "an integrity failure surfaces as RTE_E_INVALID");
    Check(speed == 8000U, "REQ-ASW-005: the previous application value was not overwritten");
}

static void TestNeverReceivedThenTimeout(void) {
    SetUp();

    VehicleSpeedRawType speed = 1U;
    Check(Rte_Read_RpVehicleState_VehicleSpeedRaw(&speed) == RTE_E_NEVER_RECEIVED,
          "REQ-RTE-006: before any reception the status is never-received");

    DeliverVehicleStatus(5000U, 1U);
    Check(Rte_Read_RpVehicleState_VehicleSpeedRaw(&speed) == RTE_E_OK, "then it becomes OK");

    ComFake_SetRxTimeout((Com_IPduIdType)RTE_IPDU_RX_VEHICLE_STATUS, TRUE);
    Check(Rte_Read_RpVehicleState_VehicleSpeedRaw(&speed) == RTE_E_TIMEOUT,
          "a missed deadline surfaces as RTE_E_TIMEOUT");
}

static void TestDisplayPortReachesTheAbstraction(void) {
    SetUp();

    Check(Rte_Call_RpDisplay_SetClusterDisplay(CLUSTER_STATE_NORMAL, 4200U, 0U) == RTE_E_OK,
          "a display call succeeds");
    IoHwAb_ClusterDisplayType last;
    Check(IoHwAbFake_GetLastDisplay(&last) == TRUE, "the abstraction received a display value");
    Check(last.state == CLUSTER_STATE_NORMAL, "the state reached the abstraction");
    Check(last.displayedSpeedRaw == 4200U, "the speed reached the abstraction");

    IoHwAbFake_SetDisplayResult(E_NOT_OK);
    Check(Rte_Call_RpDisplay_SetClusterDisplay(CLUSTER_STATE_NORMAL, 1U, 0U) == RTE_E_NOT_OK,
          "a failed display write is reported to the caller");
}

static void TestTaskDrivesTheSwc(void) {
    SetUp();
    DeliverVehicleStatus(3000U, 1U);

    /* Three valid cycles are what REQ-ASW-004 needs to leave COMM_FAULT. */
    Rte_Task_10ms();
    Rte_Task_10ms();
    Rte_Task_10ms();
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_NORMAL,
          "the task entry function actually ran the SW-C's runnable");

    uint16 sent = 0U;
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_CLUSTER_STATE, &sent);
    Check(sent == (uint16)CLUSTER_STATE_NORMAL, "the SW-C's output reached COM through the RTE");

    uint32 displayBefore = IoHwAbFake_GetDisplayCallCount();
    Rte_Task_100ms();
    Check(IoHwAbFake_GetDisplayCallCount() == displayBefore + 1U,
          "the 100 ms task refreshes the display exactly once");
}

static void TestNullPointersRejected(void) {
    SetUp();
    uint32 detBefore = Det_GetTotalErrorCount();

    Check(Rte_Read_RpVehicleState_VehicleSpeedRaw(NULL) == RTE_E_NOT_OK,
          "REQ-RTE-001: a null speed pointer is rejected");
    Check(Rte_Read_RpVehicleState_IgnitionOn(NULL) == RTE_E_NOT_OK,
          "a null ignition pointer is rejected");
    Check(Det_GetTotalErrorCount() == detBefore + 2U,
          "each rejected pointer produced exactly one Det record");
}

static void TestCallsBeforeStartRejected(void) {
    Det_Init();
    (void)Rte_Stop();

    Check(Rte_Write_PpClusterStatus_ClusterState(CLUSTER_STATE_OFF) == RTE_E_NOT_OK,
          "a write before Rte_Start is rejected");
    Check(Rte_Call_RpDisplay_SetClusterDisplay(CLUSTER_STATE_OFF, 0U, 0U) == RTE_E_NOT_OK,
          "a display call before Rte_Start is rejected");
}

int main(void) {
    TestWriteMapsOntoComSignals();
    TestRxUpdateIsAtomicForTheSwc();
    TestInvalidFrameDoesNotOverwrite();
    TestNeverReceivedThenTimeout();
    TestDisplayPortReachesTheAbstraction();
    TestTaskDrivesTheSwc();
    TestNullPointersRejected();
    TestCallsBeforeStartRejected();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_rte_cluster: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_rte_cluster: all checks passed\n");
    return 0;
}
