#include "IoHwAb_Fake.h"

#include <stddef.h>

static VehicleSpeedRawType g_speed;
static IoHwAb_ValidityType g_speedValidity = IOHWAB_INVALID;
static Std_ReturnType g_speedResult = E_OK;

static IgnitionOnType g_ignition;
static IoHwAb_ValidityType g_ignitionValidity = IOHWAB_INVALID;
static Std_ReturnType g_ignitionResult = E_OK;

static Std_ReturnType g_displayResult = E_OK;
static uint32 g_displayCallCount;
static IoHwAb_ClusterDisplayType g_lastDisplay;
static boolean g_hasDisplay;

Std_ReturnType IoHwAb_GetVehicleSpeedRaw(VehicleSpeedRawType *speed_raw,
                                         IoHwAb_ValidityType *validity) {
    if ((speed_raw == NULL) || (validity == NULL)) {
        return E_NOT_OK;
    }
    *speed_raw = g_speed;
    *validity = g_speedValidity;
    return g_speedResult;
}

Std_ReturnType IoHwAb_GetIgnitionState(IgnitionOnType *ignition_on, IoHwAb_ValidityType *validity) {
    if ((ignition_on == NULL) || (validity == NULL)) {
        return E_NOT_OK;
    }
    *ignition_on = g_ignition;
    *validity = g_ignitionValidity;
    return g_ignitionResult;
}

Std_ReturnType IoHwAb_SetClusterDisplay(const IoHwAb_ClusterDisplayType *value) {
    if (value == NULL) {
        return E_NOT_OK;
    }
    g_lastDisplay = *value;
    g_hasDisplay = TRUE;
    g_displayCallCount++;
    return g_displayResult;
}

void IoHwAbFake_Reset(void) {
    g_speed = 0U;
    g_speedValidity = IOHWAB_INVALID;
    g_speedResult = E_OK;
    g_ignition = FALSE;
    g_ignitionValidity = IOHWAB_INVALID;
    g_ignitionResult = E_OK;
    g_displayResult = E_OK;
    g_displayCallCount = 0U;
    g_hasDisplay = FALSE;
}

void IoHwAbFake_SetSpeed(VehicleSpeedRawType speed, IoHwAb_ValidityType validity,
                         Std_ReturnType result) {
    g_speed = speed;
    g_speedValidity = validity;
    g_speedResult = result;
}

void IoHwAbFake_SetIgnition(IgnitionOnType ignition, IoHwAb_ValidityType validity,
                            Std_ReturnType result) {
    g_ignition = ignition;
    g_ignitionValidity = validity;
    g_ignitionResult = result;
}

void IoHwAbFake_SetDisplayResult(Std_ReturnType result) {
    g_displayResult = result;
}

uint32 IoHwAbFake_GetDisplayCallCount(void) {
    return g_displayCallCount;
}

boolean IoHwAbFake_GetLastDisplay(IoHwAb_ClusterDisplayType *out) {
    if ((out == NULL) || (!g_hasDisplay)) {
        return FALSE;
    }
    *out = g_lastDisplay;
    return TRUE;
}
