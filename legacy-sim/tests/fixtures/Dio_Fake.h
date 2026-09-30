/**
 * @file Dio_Fake.h
 * @brief Test double for Dio (bsw/mcal/Dio), used by test_iohwab.
 */
#ifndef DIO_FAKE_H
#define DIO_FAKE_H

#include "Dio.h"

void DioFake_Reset(void);
void DioFake_SetChannelLevel(Dio_ChannelType channel, Dio_LevelType level);
void DioFake_SetChannelAge(Dio_ChannelType channel, Gpt_ValueType ageMs);

#endif /* DIO_FAKE_H */
