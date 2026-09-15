/**
 * @file IoHwAb_Fake.h
 * @brief Test double for IoHwAb, used by the RTE tests.
 *
 * Implements only what the RTE actually calls — the two input getters and
 * the display setter — following the same "implement only what's called"
 * convention as the other fixtures.
 */
#ifndef IOHWAB_FAKE_H
#define IOHWAB_FAKE_H

#include "IoHwAb.h"

void IoHwAbFake_Reset(void);
/** Stages what the next IoHwAb_GetVehicleSpeedRaw returns. */
void IoHwAbFake_SetSpeed(VehicleSpeedRawType speed, IoHwAb_ValidityType validity,
                         Std_ReturnType result);
/** Stages what the next IoHwAb_GetIgnitionState returns. */
void IoHwAbFake_SetIgnition(IgnitionOnType ignition, IoHwAb_ValidityType validity,
                            Std_ReturnType result);
/** Makes every subsequent IoHwAb_SetClusterDisplay return this value. */
void IoHwAbFake_SetDisplayResult(Std_ReturnType result);

uint32 IoHwAbFake_GetDisplayCallCount(void);
boolean IoHwAbFake_GetLastDisplay(IoHwAb_ClusterDisplayType *out);

#endif /* IOHWAB_FAKE_H */
