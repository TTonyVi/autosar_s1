/**
 * @file Adc_Fake.h
 * @brief Test double for Adc (bsw/mcal/Adc), used by test_iohwab.
 *
 * Implements the full public Adc.h API over in-memory state so IoHwAb's
 * scaling, plausibility and staleness logic can be driven exactly, with no
 * socket and no real time. Per spec section 12.1 it is never linked into a
 * production ECU target.
 */
#ifndef ADC_FAKE_H
#define ADC_FAKE_H

#include "Adc.h"

void AdcFake_Reset(void);
/** Stages the raw counts a later Adc_ReadGroup returns, and marks the group converted. */
void AdcFake_SetGroupValue(Adc_GroupType group, Adc_ValueGroupType counts);
/** Stages the age Adc_GetGroupAge reports for the group. */
void AdcFake_SetGroupAge(Adc_GroupType group, Gpt_ValueType ageMs);
/** Forces the group back to ADC_IDLE (nothing ever received). */
void AdcFake_SetGroupIdle(Adc_GroupType group);

#endif /* ADC_FAKE_H */
