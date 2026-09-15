/**
 * @file IoHwAb_Cfg.c
 * @brief The static IoHwAb configuration instances, one per ECU role.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 11.2: configuration is
 * static, const, and file-scope. Generic module implementation is shared by
 * both ECUs and only the configuration differs (`REQ-ARCH-005`) — which is
 * exactly what the two instances below express.
 */
#include "IoHwAb_Cfg.h"

#include <stddef.h>

#include "IoHwAb.h"

static const IoHwAb_SignalConfigType g_sensorSignals[IOHWAB_NUM_SIGNALS] = {
    {
        .signalId = IOHWAB_SIGNAL_VEHICLE_SPEED,
        .sourceDriver = IOHWAB_SOURCE_ADC,
        .sourceChannel = (uint8)IOHWAB_ADC_GROUP_WHEEL_SPEED,
        .scalingNumerator = IOHWAB_SPEED_SCALING_NUMERATOR,
        .scalingDenominator = IOHWAB_SPEED_SCALING_DENOMINATOR,
        .offset = IOHWAB_SPEED_OFFSET,
        .outputMin = IOHWAB_SPEED_OUTPUT_MIN,
        .outputMax = IOHWAB_SPEED_OUTPUT_MAX,
        .plausibilityMinCounts = IOHWAB_SPEED_PLAUSIBILITY_MIN_COUNTS,
        .plausibilityMaxCounts = IOHWAB_SPEED_PLAUSIBILITY_MAX_COUNTS,
        .inputTimeoutMs = IOHWAB_INPUT_TIMEOUT_MS,
    },
    {
        .signalId = IOHWAB_SIGNAL_IGNITION,
        .sourceDriver = IOHWAB_SOURCE_DIO,
        .sourceChannel = (uint8)IOHWAB_DIO_CHANNEL_IGNITION,
        /* A digital level needs no scaling: 1:1, no offset, 0..1. */
        .scalingNumerator = 1U,
        .scalingDenominator = 1U,
        .offset = 0,
        .outputMin = 0U,
        .outputMax = 1U,
        .plausibilityMinCounts = 0U,
        .plausibilityMaxCounts = 1U,
        .inputTimeoutMs = IOHWAB_INPUT_TIMEOUT_MS,
    },
};

/** SensorEcu: both inputs wired, no display panel. */
const IoHwAb_ConfigType IoHwAb_ConfigSensor = {
    .signals = g_sensorSignals,
    .numSignals = (uint8)IOHWAB_NUM_SIGNALS,
    .hasDisplay = FALSE,
    .displayTimeoutMs = IOHWAB_DISPLAY_TIMEOUT_MS,
};

/** ClusterEcu: no local sensor wiring, one display panel. */
const IoHwAb_ConfigType IoHwAb_ConfigCluster = {
    .signals = NULL,
    .numSignals = 0U,
    .hasDisplay = TRUE,
    .displayTimeoutMs = IOHWAB_DISPLAY_TIMEOUT_MS,
};
