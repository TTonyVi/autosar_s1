/**
 * @file test_iohwab.c
 * @brief Unit tests for IoHwAb (bsw/ecu_abstraction/IoHwAb).
 *
 * Covers spec section 14.1 row "test_iohwab": conversion vectors at 0,
 * 2048 and 4095 counts, rounding, out-of-range counts, timeout
 * invalidity, ignition mapping, and display value assembly.
 *
 * Exercises the REAL IoHwAb against FAKE Adc/Dio/Display/Gpt and the REAL
 * Dem, so every input is staged exactly and the DEM consequences of
 * REQ-IOHWAB-004 are observable.
 */
#include <stdio.h>
#include <string.h>

#include "Adc_Fake.h"
#include "Dem.h"
#include "Det.h"
#include "Dio_Fake.h"
#include "Display_Fake.h"
#include "Gpt_Fake.h"
#include "IoHwAb.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void SetUpSensor(void) {
    Det_Init();
    Dem_Init();
    GptFake_Reset();
    Gpt_Init(NULL);
    AdcFake_Reset();
    DioFake_Reset();
    IoHwAb_Init(&IoHwAb_ConfigSensor);
}

static void SetUpCluster(void) {
    Det_Init();
    Dem_Init();
    GptFake_Reset();
    Gpt_Init(NULL);
    DisplayFake_Reset();
    IoHwAb_Init(&IoHwAb_ConfigCluster);
}

/** Stages one fresh, plausible ADC sample and runs one sampling cycle. */
static VehicleSpeedRawType SampleSpeed(Adc_ValueGroupType counts, Gpt_ValueType ageMs,
                                       IoHwAb_ValidityType *validityOut) {
    AdcFake_SetGroupValue((Adc_GroupType)IOHWAB_ADC_GROUP_WHEEL_SPEED, counts);
    AdcFake_SetGroupAge((Adc_GroupType)IOHWAB_ADC_GROUP_WHEEL_SPEED, ageMs);
    IoHwAb_MainFunction();

    VehicleSpeedRawType speed = 0xFFFFU;
    IoHwAb_ValidityType validity = IOHWAB_INVALID;
    (void)IoHwAb_GetVehicleSpeedRaw(&speed, &validity);
    if (validityOut != NULL) {
        *validityOut = validity;
    }
    return speed;
}

static void TestConversionVectors(void) {
    SetUpSensor();
    IoHwAb_ValidityType validity = IOHWAB_INVALID;

    /* Section 4.1.2: speed_raw = (adc_counts * 25000) / 4095. */
    Check(SampleSpeed(0U, 0U, &validity) == 0U, "0 counts converts to 0.00 km/h");
    Check(validity == IOHWAB_VALID, "a fresh in-range 0-count sample is valid");

    Check(SampleSpeed(2048U, 0U, &validity) == 12503U,
          "2048 counts converts to 12503 (125.03 km/h), truncated");
    Check(validity == IOHWAB_VALID, "the mid-scale sample is valid");

    Check(SampleSpeed(4095U, 0U, &validity) == 25000U,
          "4095 counts converts to full scale 25000 (250.00 km/h)");
    Check(validity == IOHWAB_VALID, "the full-scale sample is valid");
}

static void TestRoundingTruncatesTowardZero(void) {
    SetUpSensor();
    /* 1 count is 25000/4095 = 6.105..., and integer division truncates. A
     * rounding change would silently shift every speed on the bus. */
    Check(SampleSpeed(1U, 0U, NULL) == 6U, "1 count truncates to 6, not 7");
    Check(SampleSpeed(3U, 0U, NULL) == 18U, "3 counts truncate to 18 (18.31 exact)");
}

static void TestOutOfRangeCountsAreInvalidAndReported(void) {
    SetUpSensor();
    IoHwAb_ValidityType validity = IOHWAB_VALID;

    VehicleSpeedRawType speed = SampleSpeed(5000U, 0U, &validity);
    Check(validity == IOHWAB_INVALID, "counts above the plausibility window are invalid");
    Check(speed == 0U, "an implausible sample yields 0, never a scaled bogus value");

    Dem_EventRecordType record;
    (void)memset(&record, 0, sizeof(record));
    Check(Dem_GetEventRecord(DEM_EVENT_SPEED_SENSOR_RANGE, &record) == E_OK,
          "the range event is readable");
    Check(record.currentStatus == DEM_EVENT_STATUS_FAILED,
          "REQ-IOHWAB-004: implausible counts fail DEM_EVENT_SPEED_SENSOR_RANGE");
}

static void TestStaleInputIsInvalidAndReported(void) {
    SetUpSensor();
    IoHwAb_ValidityType validity = IOHWAB_VALID;

    /* REQ-IOHWAB-002: nothing fresh within IOHWAB_INPUT_TIMEOUT_MS. */
    VehicleSpeedRawType speed = SampleSpeed(2048U, IOHWAB_INPUT_TIMEOUT_MS + 1U, &validity);
    Check(validity == IOHWAB_INVALID, "a sample older than the timeout is invalid");
    Check(speed == 0U, "REQ-IOHWAB-001: a stale value is never handed back as a number");

    Dem_EventRecordType record;
    (void)memset(&record, 0, sizeof(record));
    Check(Dem_GetEventRecord(DEM_EVENT_SENSOR_LINK_LOST, &record) == E_OK,
          "the link event is readable");
    Check(record.currentStatus == DEM_EVENT_STATUS_FAILED,
          "a stale input fails DEM_EVENT_SENSOR_LINK_LOST");

    /* Exactly at the timeout is still fresh; one millisecond past is not. */
    (void)SampleSpeed(2048U, IOHWAB_INPUT_TIMEOUT_MS, &validity);
    Check(validity == IOHWAB_VALID, "a sample exactly at the timeout is still valid");
}

static void TestNeverSampledInputIsInvalid(void) {
    SetUpSensor();
    /* Nothing staged at all: Adc is ADC_IDLE. */
    IoHwAb_MainFunction();

    VehicleSpeedRawType speed = 0xFFFFU;
    IoHwAb_ValidityType validity = IOHWAB_VALID;
    (void)IoHwAb_GetVehicleSpeedRaw(&speed, &validity);
    Check(validity == IOHWAB_INVALID, "an input that never arrived is invalid");
    Check(speed == 0U, "an input that never arrived reads as 0");
}

static void TestIgnitionMapping(void) {
    SetUpSensor();
    IgnitionOnType ignition = TRUE;
    IoHwAb_ValidityType validity = IOHWAB_INVALID;

    DioFake_SetChannelLevel((Dio_ChannelType)IOHWAB_DIO_CHANNEL_IGNITION, STD_LOW);
    DioFake_SetChannelAge((Dio_ChannelType)IOHWAB_DIO_CHANNEL_IGNITION, 0U);
    IoHwAb_MainFunction();
    (void)IoHwAb_GetIgnitionState(&ignition, &validity);
    Check(ignition == FALSE, "STD_LOW maps to ignition off");
    Check(validity == IOHWAB_VALID, "a fresh level is valid");

    DioFake_SetChannelLevel((Dio_ChannelType)IOHWAB_DIO_CHANNEL_IGNITION, STD_HIGH);
    IoHwAb_MainFunction();
    (void)IoHwAb_GetIgnitionState(&ignition, &validity);
    Check(ignition == TRUE, "STD_HIGH maps to ignition on");

    /* A stale level is invalid and reads as off, never as a frozen on. */
    DioFake_SetChannelAge((Dio_ChannelType)IOHWAB_DIO_CHANNEL_IGNITION,
                          IOHWAB_INPUT_TIMEOUT_MS + 1U);
    IoHwAb_MainFunction();
    (void)IoHwAb_GetIgnitionState(&ignition, &validity);
    Check(validity == IOHWAB_INVALID, "a stale ignition level is invalid");
    Check(ignition == FALSE, "a stale ignition level reads as off");
}

static void TestDisplayValueAssembly(void) {
    SetUpCluster();

    IoHwAb_ClusterDisplayType value;
    value.state = CLUSTER_STATE_OVERSPEED;
    value.displayedSpeedRaw = 12345U;
    value.flags = CLUSTER_FLAG_OVERSPEED;

    GptFake_SetTimeMs(4242U);
    Check(IoHwAb_SetClusterDisplay(&value) == E_OK, "a display write succeeds");

    char frame[128];
    Check(DisplayFake_GetLastFrame(frame, (uint16)sizeof(frame)) == TRUE, "a frame was written");
    /* Section 15.5.2: STATE=<..>;SPEED=<..>;FLAGS=0x<HH>;T=<ms>, fixed order. */
    Check(strcmp(frame, "STATE=OVERSPEED;SPEED=12345;FLAGS=0x01;T=4242") == 0,
          "the display frame matches the section 15.5.2 format verbatim");
}

static void TestDisplayStateNamesAndClamping(void) {
    SetUpCluster();
    char frame[128];

    IoHwAb_ClusterDisplayType value;
    value.state = CLUSTER_STATE_COMM_FAULT;
    value.displayedSpeedRaw = 30000U; /* above full scale */
    value.flags = CLUSTER_FLAG_TIMEOUT | CLUSTER_FLAG_INTEGRITY;
    GptFake_SetTimeMs(7U);
    (void)IoHwAb_SetClusterDisplay(&value);
    (void)DisplayFake_GetLastFrame(frame, (uint16)sizeof(frame));
    Check(strcmp(frame, "STATE=COMM_FAULT;SPEED=25000;FLAGS=0x06;T=7") == 0,
          "an over-range speed is clamped to full scale and flags are masked to bits 0..2");

    value.state = CLUSTER_STATE_OFF;
    value.displayedSpeedRaw = 0U;
    value.flags = 0U;
    (void)IoHwAb_SetClusterDisplay(&value);
    (void)DisplayFake_GetLastFrame(frame, (uint16)sizeof(frame));
    Check(strncmp(frame, "STATE=OFF;", 10U) == 0, "the OFF state name is written");

    value.state = CLUSTER_STATE_NORMAL;
    (void)IoHwAb_SetClusterDisplay(&value);
    (void)DisplayFake_GetLastFrame(frame, (uint16)sizeof(frame));
    Check(strncmp(frame, "STATE=NORMAL;", 13U) == 0, "the NORMAL state name is written");
}

static void TestDisplayWriteFailureIsNonFatal(void) {
    SetUpCluster();
    DisplayFake_SetWriteResult(E_NOT_OK);

    IoHwAb_ClusterDisplayType value;
    value.state = CLUSTER_STATE_NORMAL;
    value.displayedSpeedRaw = 100U;
    value.flags = 0U;

    Check(IoHwAb_SetClusterDisplay(&value) == E_NOT_OK, "a failed write is reported to the caller");
    Check(IoHwAb_GetDisplayWriteFailureCount() == 1U, "the failure is counted");
    /* REQ-VHW-005: still callable, still non-blocking, still no crash. */
    Check(IoHwAb_SetClusterDisplay(&value) == E_NOT_OK, "a second failed write is still handled");
    Check(IoHwAb_GetDisplayWriteFailureCount() == 2U, "failures keep counting");
}

static void TestInvalidCallsRejected(void) {
    SetUpSensor();

    VehicleSpeedRawType speed = 0U;
    IoHwAb_ValidityType validity = IOHWAB_VALID;
    Check(IoHwAb_GetVehicleSpeedRaw(NULL, &validity) == E_NOT_OK,
          "a null speed pointer is rejected");
    Check(IoHwAb_GetVehicleSpeedRaw(&speed, NULL) == E_NOT_OK,
          "a null validity pointer is rejected");
    Check(IoHwAb_SetClusterDisplay(NULL) == E_NOT_OK, "a null display value is rejected");

    /* The sensor ECU owns no panel: asking it to drive one is a caller error. */
    IoHwAb_ClusterDisplayType value = {CLUSTER_STATE_OFF, 0U, 0U};
    Check(IoHwAb_SetClusterDisplay(&value) == E_NOT_OK,
          "an ECU configured without a display refuses display writes");

    IoHwAb_DeInit();
    Check(IoHwAb_GetVehicleSpeedRaw(&speed, &validity) == E_NOT_OK,
          "reads after DeInit are rejected");
    Check(validity == IOHWAB_INVALID, "a rejected read still reports invalid, never valid");
}

int main(void) {
    TestConversionVectors();
    TestRoundingTruncatesTowardZero();
    TestOutOfRangeCountsAreInvalidAndReported();
    TestStaleInputIsInvalidAndReported();
    TestNeverSampledInputIsInvalid();
    TestIgnitionMapping();
    TestDisplayValueAssembly();
    TestDisplayStateNamesAndClamping();
    TestDisplayWriteFailureIsNonFatal();
    TestInvalidCallsRejected();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_iohwab: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_iohwab: all checks passed\n");
    return 0;
}
