/**
 * @file test_adc_dio.c
 * @brief Phase 2 unit tests for Adc and Dio (bsw/mcal/Adc, bsw/mcal/Dio).
 *
 * Covers section 14.1 ("test_adc_dio"): raw count pass-through without
 * scaling, staleness age reporting, unknown channel, default level before
 * first datagram. Uses the fake VirtualHw and fake Gpt (tests/fixtures)
 * so timing and "the latest payload" are both fully controlled —
 * deterministic regardless of host speed.
 */
#include <stdio.h>

#include "Adc.h"
#include "Det.h"
#include "Dio.h"
#include "Gpt_Fake.h"
#include "Platform_Types.h"
#include "VirtualHw_Fake.h"

#define SENSOR_LINK_ID 0U
#define ADC_GROUP_WHEEL_SPEED 0U
#define DIO_CHANNEL_IGNITION 0U

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static const Adc_GroupConfigType g_adcGroups[1] = {
    {ADC_GROUP_WHEEL_SPEED, 0U, 12U, SENSOR_LINK_ID, 0U, 4095U},
};
static const Adc_ConfigType g_adcConfig = {g_adcGroups, 1U};

static const Dio_ChannelConfigType g_dioChannels[1] = {
    {DIO_CHANNEL_IGNITION, SENSOR_LINK_ID, STD_LOW},
};
static const Dio_ConfigType g_dioConfig = {g_dioChannels, 1U};

static void ResetSystem(void) {
    GptFake_Reset();
    Gpt_Init(NULL);
    Det_Init();
    VirtualHwFake_Reset();
    Adc_Init(&g_adcConfig);
    Dio_Init(&g_dioConfig);
}

static void TestRawPassThroughNoScaling(void) {
    ResetSystem();

    /* 4095 counts is full scale (250.00 km/h once IoHwAb scales it in
     * Phase 5); Adc must return exactly 4095, not a scaled/clamped value. */
    VirtualHwFake_Deliver(SENSOR_LINK_ID, "ADC0=4095;DIO0=1;SEQ=1");

    Adc_ValueGroupType speedRaw = 0U;
    Std_ReturnType r = Adc_ReadGroup(ADC_GROUP_WHEEL_SPEED, &speedRaw);
    Check(r == E_OK, "Adc_ReadGroup succeeds once a datagram has arrived");
    Check(speedRaw == 4095U, "the raw count passes through unchanged (no scaling/clamping)");
    Check(Adc_GetGroupStatus(ADC_GROUP_WHEEL_SPEED) == ADC_STREAM_CONVERTED,
          "status reflects a received conversion result");

    Dio_LevelType ignition = Dio_ReadChannel(DIO_CHANNEL_IGNITION);
    Check(ignition == STD_HIGH, "Dio passes the raw level through unchanged");

    /* An implausibly out-of-range count (real ADC is only 12-bit, 0..4095)
     * is still passed straight through — plausibility is IoHwAb's job.
     * 60000 still fits in the uint16 Adc_ValueGroupType, so this isolates
     * "not clamped to 0..4095" from any incidental integer truncation. */
    VirtualHwFake_Deliver(SENSOR_LINK_ID, "ADC0=60000;DIO0=0;SEQ=2");
    r = Adc_ReadGroup(ADC_GROUP_WHEEL_SPEED, &speedRaw);
    Check(r == E_OK, "Adc_ReadGroup still succeeds for an out-of-range count");
    Check(speedRaw == 60000U, "an implausible count is passed through unclamped (REQ-MCAL-001)");
}

static void TestStalenessAgeReporting(void) {
    ResetSystem();

    VirtualHwFake_Deliver(SENSOR_LINK_ID, "ADC0=2048;DIO0=1;SEQ=1");
    Check(Adc_GetGroupAge(ADC_GROUP_WHEEL_SPEED) == 0U, "age is 0ms right at delivery");
    Check(Dio_GetChannelAge(DIO_CHANNEL_IGNITION) == 0U, "Dio's age is also 0ms right at delivery");

    GptFake_AdvanceMs(250U);
    Check(Adc_GetGroupAge(ADC_GROUP_WHEEL_SPEED) == 250U,
          "age grows with simulated time, no new datagram needed");
    Check(Dio_GetChannelAge(DIO_CHANNEL_IGNITION) == 250U,
          "Dio's age grows identically, sharing the same link");

    GptFake_AdvanceMs(1000U);
    Check(Adc_GetGroupAge(ADC_GROUP_WHEEL_SPEED) == 1250U,
          "age keeps accumulating past any particular threshold");
}

static void TestUnknownChannelRejected(void) {
    ResetSystem();

    Adc_ValueGroupType value = 0U;
    uint32 detBefore = Det_GetTotalErrorCount();
    Std_ReturnType r = Adc_ReadGroup((Adc_GroupType)9U, &value);
    Check(r == E_NOT_OK, "Adc_ReadGroup rejects an unconfigured group");
    Check(Det_GetTotalErrorCount() > detBefore, "the unknown group is reported to Det");

    detBefore = Det_GetTotalErrorCount();
    Dio_LevelType level = Dio_ReadChannel((Dio_ChannelType)9U);
    Check(level == STD_LOW,
          "Dio_ReadChannel returns a safe default (STD_LOW) for an unknown channel");
    Check(Det_GetTotalErrorCount() > detBefore, "the unknown channel is reported to Det");
}

static void TestDefaultLevelBeforeFirstDatagram(void) {
    ResetSystem();

    /* Nothing has been delivered yet. */
    Check(Adc_GetGroupStatus(ADC_GROUP_WHEEL_SPEED) == ADC_IDLE,
          "Adc status is IDLE before any datagram arrives");
    Adc_ValueGroupType value = 0U;
    Check(Adc_ReadGroup(ADC_GROUP_WHEEL_SPEED, &value) == E_NOT_OK,
          "Adc_ReadGroup fails cleanly before any datagram arrives");

    Dio_LevelType level = Dio_ReadChannel(DIO_CHANNEL_IGNITION);
    Check(level == STD_LOW,
          "Dio reports its configured default level (STD_LOW) before any datagram arrives");
}

int main(void) {
    TestRawPassThroughNoScaling();
    TestStalenessAgeReporting();
    TestUnknownChannelRejected();
    TestDefaultLevelBeforeFirstDatagram();

    if (g_failures == 0) {
        (void)printf("test_adc_dio: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_adc_dio: %d check(s) failed\n", g_failures);
    return 1;
}
