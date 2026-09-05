/**
 * @file Can_Fake.h
 * @brief Test double for Can (bsw/mcal/Can), used only by test_canif so
 * CanIf's own routing/validation logic can be tested deterministically
 * without a real SocketCAN interface.
 *
 * Implements the same public API as Can.h (Init, DeInit,
 * SetControllerMode, GetControllerMode, IsControllerReady, Write,
 * MainFunction_Read, MainFunction_Write, the Get*Count inspection
 * functions) backed by in-memory per-controller state, plus a
 * `CanFake_*` control surface so a test can force a controller mode or a
 * Can_Write/Can_SetControllerMode result without going through real
 * validation -- same seam pattern as tests/fixtures/VirtualHw_Fake.
 *
 * Per spec section 12.1, this file lives under tests/ and is never linked
 * into a production ECU target.
 */
#ifndef CAN_FAKE_H
#define CAN_FAKE_H

#include "Can.h" /* the API this fake substitutes for testing */

typedef struct {
    Can_HwHandleType hth;
    PduIdType swPduHandle;
    uint8 length;
    Can_IdType id;
    uint8 data[8];
} CanFake_WriteRecordType;

/** Clears all fake controller state and recorded calls. */
void CanFake_Reset(void);

/** Sets controller_id's mode directly, bypassing Can_SetControllerMode's
 * validation -- for test setup (e.g. "assume the controller is already
 * started" before exercising CanIf_Transmit). */
void CanFake_SetControllerModeDirect(uint8 controller_id, Can_ControllerStateType mode);

/** Controls what Can_SetControllerMode returns on every subsequent call
 * (default E_OK). */
void CanFake_SetSetControllerModeResult(Std_ReturnType result);

/** Controls what Can_Write returns on every subsequent call (default E_OK). */
void CanFake_SetWriteResult(Std_ReturnType result);

uint32 CanFake_GetWriteCount(void);
/** Returns FALSE if Can_Write was never called since the last reset. */
boolean CanFake_GetLastWrite(CanFake_WriteRecordType *out);

#endif /* CAN_FAKE_H */
