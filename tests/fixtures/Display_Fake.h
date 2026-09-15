/**
 * @file Display_Fake.h
 * @brief Test double for Display (bsw/mcal/Display), used by test_iohwab.
 *
 * Captures the exact bytes IoHwAb assembles, so the section 15.5.2 display
 * line format can be asserted verbatim without a socket.
 */
#ifndef DISPLAY_FAKE_H
#define DISPLAY_FAKE_H

#include "Display.h"

void DisplayFake_Reset(void);
/** Makes every subsequent Display_Write return this value. */
void DisplayFake_SetWriteResult(Std_ReturnType result);
/** Copies the last written frame as a NUL-terminated string; FALSE if none yet. */
boolean DisplayFake_GetLastFrame(char *buffer, uint16 bufferSize);
uint32 DisplayFake_GetWriteCallCount(void);

#endif /* DISPLAY_FAKE_H */
