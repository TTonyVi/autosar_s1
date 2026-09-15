/**
 * @file test_cluster_swc.c
 * @brief Unit tests for ClusterControlSwc (asw/ClusterControlSwc).
 *
 * Covers spec section 14.1 row "test_cluster_swc": all state transitions,
 * hysteresis, clamping, three-frame recovery.
 *
 * Exercises the REAL SW-C against a FAKE RTE (tests/fixtures), which is
 * what makes every branch of the section 4.4 state machine reachable
 * directly — including the RTE_E_TIMEOUT and RTE_E_INVALID statuses a real
 * COM would need many cycles to produce.
 */
#include <stdio.h>

#include "ClusterControlSwc.h"
#include "Rte_ClusterFake.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void SetUp(void) {
    RteClusterFake_Reset();
    ClusterControlSwc_Init();
}

/** Runs `cycles` 10 ms activations with the given valid remote data. */
static void RunValid(VehicleSpeedRawType speed, IgnitionOnType ignition, uint8 cycles) {
    RteClusterFake_SetVehicleState(speed, ignition, RTE_E_OK);
    for (uint8 i = 0U; i < cycles; i++) {
        ClusterControlSwc_10ms();
    }
}

/** Drives the SW-C out of COMM_FAULT into a running state (REQ-ASW-004). */
static void Recover(VehicleSpeedRawType speed, IgnitionOnType ignition) {
    RunValid(speed, ignition, CLUSTER_RECOVERY_FRAME_COUNT + 1U);
}

static void TestStartsInCommFault(void) {
    SetUp();
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_COMM_FAULT,
          "the SW-C initializes to COMM_FAULT");
    Check(ClusterControlSwc_GetStatusFlags() == CLUSTER_FLAG_TIMEOUT,
          "the initial flags show the communication timeout");
}

static void TestThreeFrameRecovery(void) {
    SetUp();
    RteClusterFake_SetVehicleState(5000U, TRUE, RTE_E_OK);

    /* REQ-ASW-004: three consecutive valid receptions are required. The
     * third cycle is the one that satisfies the counter; the state changes
     * on the cycle after the counter reaches the threshold. */
    ClusterControlSwc_10ms();
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_COMM_FAULT,
          "one valid frame is not enough to leave COMM_FAULT");
    ClusterControlSwc_10ms();
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_COMM_FAULT,
          "two valid frames are not enough either");
    ClusterControlSwc_10ms();
    Check(ClusterControlSwc_GetConsecutiveValidCount() == CLUSTER_RECOVERY_FRAME_COUNT,
          "three valid frames satisfy the recovery counter");
    ClusterControlSwc_10ms();
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_NORMAL,
          "after three valid frames the cluster recovers");
}

static void TestRecoveryTargetDependsOnData(void) {
    SetUp();
    Recover(0U, FALSE);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OFF,
          "recovery with ignition off lands in OFF");

    SetUp();
    Recover(20000U, TRUE);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OVERSPEED,
          "recovery above the threshold lands directly in OVERSPEED");

    SetUp();
    Recover(5000U, TRUE);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_NORMAL,
          "recovery below the threshold lands in NORMAL");
}

static void TestOffToNormalOnIgnition(void) {
    SetUp();
    Recover(0U, FALSE);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OFF, "starting point is OFF");

    RunValid(0U, TRUE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_NORMAL, "ignition on moves OFF to NORMAL");
}

static void TestNormalAndOverspeedToOff(void) {
    SetUp();
    Recover(5000U, TRUE);
    RunValid(5000U, FALSE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OFF, "ignition off moves NORMAL to OFF");

    SetUp();
    Recover(20000U, TRUE);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OVERSPEED, "starting point is OVERSPEED");
    RunValid(20000U, FALSE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OFF, "ignition off moves OVERSPEED to OFF");
}

static void TestOverspeedHysteresis(void) {
    SetUp();
    Recover(5000U, TRUE);

    /* REQ-ASW-002: entry at exactly 10000 raw (100.00 km/h). */
    RunValid((VehicleSpeedRawType)(CLUSTER_OVERSPEED_ENTRY_RAW - 1U), TRUE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_NORMAL,
          "one unit below the entry threshold stays NORMAL");
    RunValid(CLUSTER_OVERSPEED_ENTRY_RAW, TRUE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OVERSPEED,
          "exactly at the entry threshold enters OVERSPEED");

    /* REQ-ASW-003: exit only at or below 9700 raw, not at the entry point. */
    RunValid((VehicleSpeedRawType)(CLUSTER_OVERSPEED_EXIT_RAW + 1U), TRUE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OVERSPEED,
          "inside the hysteresis band the cluster stays OVERSPEED");
    RunValid(CLUSTER_OVERSPEED_EXIT_RAW, TRUE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_NORMAL,
          "exactly at the exit threshold returns to NORMAL");
}

static void TestCommunicationInvalidityOutranksEverything(void) {
    /* REQ-ASW-001 for each way communication can be untrustworthy. */
    const Rte_ReturnType badStatuses[3] = {RTE_E_TIMEOUT, RTE_E_INVALID, RTE_E_NEVER_RECEIVED};
    const ClusterStatusFlagsType expectedFlag[3] = {CLUSTER_FLAG_TIMEOUT, CLUSTER_FLAG_INTEGRITY,
                                                    CLUSTER_FLAG_TIMEOUT};

    for (uint8 i = 0U; i < 3U; i++) {
        SetUp();
        Recover(20000U, TRUE);
        Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OVERSPEED, "starting point is running");

        RteClusterFake_SetVehicleState(20000U, TRUE, badStatuses[i]);
        ClusterControlSwc_10ms();
        Check(ClusterControlSwc_GetState() == CLUSTER_STATE_COMM_FAULT,
              "invalid communication forces COMM_FAULT regardless of speed and ignition");
        Check((ClusterControlSwc_GetStatusFlags() & expectedFlag[i]) != 0U,
              "the matching status flag is raised");
        Check(ClusterControlSwc_GetDisplayedSpeedRaw() == 0U,
              "COMM_FAULT shows 0, never the last received speed");
        Check(ClusterControlSwc_GetConsecutiveValidCount() == 0U,
              "the recovery counter restarts from zero");
    }
}

static void TestDisplayedSpeedClamping(void) {
    SetUp();
    Recover(5000U, TRUE);

    /* REQ-ASW-006: displayed speed is clamped to 250.00 km/h. */
    RunValid(60000U, TRUE, 1U);
    Check(ClusterControlSwc_GetDisplayedSpeedRaw() == VEHICLE_SPEED_RAW_MAX,
          "an over-range speed is clamped to full scale");

    RunValid(7000U, TRUE, 1U);
    Check(ClusterControlSwc_GetDisplayedSpeedRaw() == 7000U,
          "an in-range speed is displayed unchanged");

    RunValid(7000U, FALSE, 1U);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OFF, "ignition off moves to OFF");
    Check(ClusterControlSwc_GetDisplayedSpeedRaw() == 0U, "an OFF cluster displays 0");
}

static void TestOverspeedFlagTracksState(void) {
    SetUp();
    Recover(5000U, TRUE);
    Check((ClusterControlSwc_GetStatusFlags() & CLUSTER_FLAG_OVERSPEED) == 0U,
          "NORMAL does not raise the overspeed flag");

    RunValid(20000U, TRUE, 1U);
    Check((ClusterControlSwc_GetStatusFlags() & CLUSTER_FLAG_OVERSPEED) != 0U,
          "OVERSPEED raises the overspeed flag");
    Check(ClusterControlSwc_GetStatusFlags() <= CLUSTER_FLAGS_VALID_MASK,
          "bits 3..7 are never set (section 4.3)");

    RunValid(1000U, TRUE, 1U);
    Check((ClusterControlSwc_GetStatusFlags() & CLUSTER_FLAG_OVERSPEED) == 0U,
          "returning to NORMAL clears the overspeed flag");
}

static void TestEveryCycleWritesAllThreePortElements(void) {
    SetUp();
    RteClusterFake_SetVehicleState(5000U, TRUE, RTE_E_OK);
    ClusterControlSwc_10ms();
    Check(RteClusterFake_GetWriteCount() == 3U,
          "one activation publishes state, speed and flags exactly once each");
}

static void TestDisplayFailureDoesNotChangeState(void) {
    SetUp();
    Recover(20000U, TRUE);
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OVERSPEED, "starting point is OVERSPEED");

    /* Section 4.2.1: a display write failure SHALL NOT change the state. */
    RteClusterFake_SetDisplayResult(RTE_E_NOT_OK);
    ClusterControlSwc_100ms();
    ClusterControlSwc_100ms();
    Check(ClusterControlSwc_GetState() == CLUSTER_STATE_OVERSPEED,
          "repeated display failures leave the state machine untouched");
    Check(RteClusterFake_GetDisplayCallCount() == 2U,
          "the SW-C keeps refreshing the panel despite the failures");
    Check(RteClusterFake_GetDisplayState() == CLUSTER_STATE_OVERSPEED,
          "the panel is still offered the current state");
}

int main(void) {
    TestStartsInCommFault();
    TestThreeFrameRecovery();
    TestRecoveryTargetDependsOnData();
    TestOffToNormalOnIgnition();
    TestNormalAndOverspeedToOff();
    TestOverspeedHysteresis();
    TestCommunicationInvalidityOutranksEverything();
    TestDisplayedSpeedClamping();
    TestOverspeedFlagTracksState();
    TestEveryCycleWritesAllThreePortElements();
    TestDisplayFailureDoesNotChangeState();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_cluster_swc: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_cluster_swc: all checks passed\n");
    return 0;
}
