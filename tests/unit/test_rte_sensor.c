/**
 * @file test_rte_sensor.c
 * @brief Unit tests for the SensorEcu RTE (rte/generated/SensorEcu).
 *
 * Covers spec section 14.1 row "test_rte_sensor": writes, service call,
 * never-received, timeout, null pointers.
 *
 * Exercises the REAL RTE and the REAL VehicleStateSwc against a FAKE Com
 * and a FAKE IoHwAb, so both the signal mapping (REQ-RTE-004) and the
 * invalid-input policy (REQ-IOHWAB-003) are observable from the outside.
 */
#include <stdio.h>

#include "Com_Fake.h"
#include "Det.h"
#include "IoHwAb_Fake.h"
#include "Rte_SensorEcu.h"
#include "Rte_SensorEcu_Cfg.h"
#include "Rte_VehicleStateSwc.h"
#include "VehicleStateSwc.h"

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

static void TestWriteMapsOntoComSignals(void) {
    SetUp();

    Check(Rte_Write_PpVehicleState_VehicleSpeedRaw(12345U) == RTE_E_OK, "a speed write succeeds");
    Check(Rte_Write_PpVehicleState_IgnitionOn(TRUE) == RTE_E_OK, "an ignition write succeeds");

    uint16 sent = 0U;
    /* REQ-RTE-004: the write updates the RTE value and then calls
     * Com_SendSignal through the generated mapping. */
    Check(ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_VEHICLE_SPEED_RAW, &sent) ==
              TRUE,
          "the speed reached a COM signal");
    Check(sent == 12345U, "the speed reached COM unchanged");
    Check(ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_IGNITION_ON, &sent) == TRUE,
          "the ignition reached a COM signal");
    Check(sent == 1U, "TRUE maps to 1 on the wire");

    (void)Rte_Write_PpVehicleState_IgnitionOn(FALSE);
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_IGNITION_ON, &sent);
    Check(sent == 0U, "FALSE maps to 0 on the wire");
}

static void TestWriteFailureIsForwarded(void) {
    SetUp();
    ComFake_SetSendSignalResult(E_NOT_OK);
    Check(Rte_Write_PpVehicleState_VehicleSpeedRaw(1U) == RTE_E_NOT_OK,
          "a COM failure is reported to the caller, not swallowed");
    ComFake_SetSendSignalResult(E_OK);
}

static void TestServiceCallReturnsValueAndValidity(void) {
    SetUp();
    IoHwAbFake_SetSpeed(9000U, IOHWAB_VALID, E_OK);
    IoHwAbFake_SetIgnition(TRUE, IOHWAB_VALID, E_OK);

    VehicleSpeedRawType speed = 0U;
    IgnitionOnType ignition = FALSE;
    Rte_InputValidityType validity = RTE_INPUT_INVALID;
    Check(Rte_Call_RpVehicleInputs_GetVehicleInputs(&speed, &ignition, &validity) == RTE_E_OK,
          "a healthy service call returns RTE_E_OK");
    Check(speed == 9000U, "the service call returns the abstraction's value");
    Check(ignition == TRUE, "the service call returns the ignition state");
    Check(validity == RTE_INPUT_VALID, "a healthy input is reported valid");
}

static void TestServiceCallPropagatesHardwareInvalidity(void) {
    SetUp();
    IoHwAbFake_SetSpeed(9000U, IOHWAB_INVALID, E_OK);
    IoHwAbFake_SetIgnition(TRUE, IOHWAB_VALID, E_OK);

    VehicleSpeedRawType speed = 0U;
    IgnitionOnType ignition = FALSE;
    Rte_InputValidityType validity = RTE_INPUT_VALID;
    Check(Rte_Call_RpVehicleInputs_GetVehicleInputs(&speed, &ignition, &validity) ==
              RTE_E_HARDWARE_INVALID,
          "one invalid input makes the whole service call hardware-invalid");
    Check(validity == RTE_INPUT_INVALID, "the validity out-parameter agrees with the return value");
}

static void TestInvalidInputPublishesZeroNotLastGood(void) {
    SetUp();

    /* One good cycle establishes a "last good" value the SW-C could freeze. */
    IoHwAbFake_SetSpeed(20000U, IOHWAB_VALID, E_OK);
    IoHwAbFake_SetIgnition(TRUE, IOHWAB_VALID, E_OK);
    Rte_Task_10ms();
    uint16 sent = 0U;
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_VEHICLE_SPEED_RAW, &sent);
    Check(sent == 20000U, "a valid input is published unchanged");

    /* Now the input goes bad. REQ-IOHWAB-003: publish 0 and ignition off,
     * never the frozen last good value. */
    IoHwAbFake_SetSpeed(20000U, IOHWAB_INVALID, E_OK);
    Rte_Task_10ms();
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_VEHICLE_SPEED_RAW, &sent);
    Check(sent == 0U, "REQ-IOHWAB-003: an invalid input publishes 0, not the last good speed");
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_IGNITION_ON, &sent);
    Check(sent == 0U, "REQ-IOHWAB-003: an invalid input publishes ignition off");
    Check(VehicleStateSwc_GetInvalidInputCount() == 1U, "the SW-C counted one invalid input");
}

static void TestSpeedIsClampedToFullScale(void) {
    SetUp();
    IoHwAbFake_SetSpeed(60000U, IOHWAB_VALID, E_OK); /* above 250.00 km/h */
    IoHwAbFake_SetIgnition(TRUE, IOHWAB_VALID, E_OK);
    Rte_Task_10ms();

    uint16 sent = 0U;
    (void)ComFake_GetLastSentSignal((Com_SignalIdType)RTE_SIGNAL_TX_VEHICLE_SPEED_RAW, &sent);
    Check(sent == 25000U, "the SW-C clamps an over-range speed to full scale");
}

static void TestNeverReceivedBeforeFirstReception(void) {
    SetUp();
    ClusterStateType state = CLUSTER_STATE_NORMAL;
    /* REQ-RTE-006: the initial remote data state is NEVER_RECEIVED. */
    Check(Rte_Read_RpClusterStatus_ClusterState(&state) == RTE_E_NEVER_RECEIVED,
          "a read before any reception reports never-received");

    DisplayedSpeedRawType speed = 1U;
    Check(Rte_Read_RpClusterStatus_DisplayedSpeedRaw(&speed) == RTE_E_NEVER_RECEIVED,
          "every element of the port reports never-received");
    ClusterStatusFlagsType flags = 1U;
    Check(Rte_Read_RpClusterStatus_StatusFlags(&flags) == RTE_E_NEVER_RECEIVED,
          "the flags element reports never-received too");
}

static void TestReceptionThenTimeout(void) {
    SetUp();

    ComFake_SetReceivedSignal((Com_SignalIdType)RTE_SIGNAL_RX_CLUSTER_STATE,
                              (uint16)CLUSTER_STATE_OVERSPEED);
    Rte_COMCbk_ClusterState();

    ClusterStateType state = CLUSTER_STATE_OFF;
    Check(Rte_Read_RpClusterStatus_ClusterState(&state) == RTE_E_OK,
          "after a reception the read is OK");
    Check(state == CLUSTER_STATE_OVERSPEED, "the received value reached the RTE buffer");

    /* COM reports the deadline missed. */
    ComFake_SetRxTimeout((Com_IPduIdType)RTE_IPDU_RX_CLUSTER_STATUS, TRUE);
    Check(Rte_Read_RpClusterStatus_ClusterState(&state) == RTE_E_TIMEOUT,
          "a missed deadline surfaces as RTE_E_TIMEOUT");
    Check(state == CLUSTER_STATE_OVERSPEED,
          "a timeout still copies the last known value; only the status changes");
}

static void TestIntegrityFailureSurfacesAsInvalid(void) {
    SetUp();
    ComFake_SetReceivedSignal((Com_SignalIdType)RTE_SIGNAL_RX_CLUSTER_STATE,
                              (uint16)CLUSTER_STATE_NORMAL);
    Rte_COMCbk_ClusterState();
    Rte_Task_10ms(); /* establishes the counter baseline */

    ComFake_SetRxCrcErrorCount((Com_IPduIdType)RTE_IPDU_RX_CLUSTER_STATUS, 1U);
    Rte_Task_10ms();

    ClusterStateType state = CLUSTER_STATE_OFF;
    Check(Rte_Read_RpClusterStatus_ClusterState(&state) == RTE_E_INVALID,
          "a frame that failed integrity validation surfaces as RTE_E_INVALID");

    /* A later good frame clears it. */
    ComFake_SetRxValidCount((Com_IPduIdType)RTE_IPDU_RX_CLUSTER_STATUS, 1U);
    Rte_Task_10ms();
    Check(Rte_Read_RpClusterStatus_ClusterState(&state) == RTE_E_OK,
          "a subsequent valid frame clears the invalid condition");
}

static void TestNullPointersRejected(void) {
    SetUp();
    uint32 detBefore = Det_GetTotalErrorCount();

    Check(Rte_Read_RpClusterStatus_ClusterState(NULL) == RTE_E_NOT_OK,
          "REQ-RTE-001: a null read pointer is rejected");
    Check(Rte_Read_RpClusterStatus_DisplayedSpeedRaw(NULL) == RTE_E_NOT_OK,
          "a null speed pointer is rejected");
    Check(Rte_Read_RpClusterStatus_StatusFlags(NULL) == RTE_E_NOT_OK,
          "a null flags pointer is rejected");

    VehicleSpeedRawType speed = 0U;
    IgnitionOnType ignition = FALSE;
    Rte_InputValidityType validity = RTE_INPUT_VALID;
    Check(Rte_Call_RpVehicleInputs_GetVehicleInputs(NULL, &ignition, &validity) == RTE_E_NOT_OK,
          "a null service-call pointer is rejected");
    Check(Rte_Call_RpVehicleInputs_GetVehicleInputs(&speed, NULL, &validity) == RTE_E_NOT_OK,
          "a null ignition pointer is rejected");
    Check(Rte_Call_RpVehicleInputs_GetVehicleInputs(&speed, &ignition, NULL) == RTE_E_NOT_OK,
          "a null validity pointer is rejected");

    Check(Det_GetTotalErrorCount() == detBefore + 6U,
          "each rejected pointer produced exactly one Det record");
}

static void TestCallsBeforeStartRejected(void) {
    Det_Init();
    (void)Rte_Stop();

    Check(Rte_Write_PpVehicleState_VehicleSpeedRaw(1U) == RTE_E_NOT_OK,
          "a write before Rte_Start is rejected");
    uint32 before = Det_GetTotalErrorCount();
    Rte_Task_10ms();
    Check(Det_GetTotalErrorCount() == before + 1U, "a task before Rte_Start reports to Det");
}

int main(void) {
    TestWriteMapsOntoComSignals();
    TestWriteFailureIsForwarded();
    TestServiceCallReturnsValueAndValidity();
    TestServiceCallPropagatesHardwareInvalidity();
    TestInvalidInputPublishesZeroNotLastGood();
    TestSpeedIsClampedToFullScale();
    TestNeverReceivedBeforeFirstReception();
    TestReceptionThenTimeout();
    TestIntegrityFailureSurfacesAsInvalid();
    TestNullPointersRejected();
    TestCallsBeforeStartRejected();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_rte_sensor: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_rte_sensor: all checks passed\n");
    return 0;
}
