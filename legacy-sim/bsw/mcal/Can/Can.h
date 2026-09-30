/**
 * @file Can.h
 * @brief Nonblocking SocketCAN transport and CAN controller state
 * (simplified Can Driver).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 6.1, 7.2, 7.4, 8.1,
 * 9.2/9.3, 11.2, 12. Together with bsw/mcal/VirtualHw/, this is the
 * complete list of files that touch a host socket in this project
 * (REQ-ARCH-009's static check) -- here, a `PF_CAN`/`SOCK_RAW`/`CAN_RAW`
 * socket bound to one SocketCAN interface per configured controller
 * (default "vcan0"; section 15.1).
 *
 * Layering note (section 3.1's table): this driver never interprets a CAN
 * ID's meaning -- it only ever moves {id, dlc, bytes} up to
 * CanIf_RxIndication / CanIf_TxConfirmation, the fixed AUTOSAR
 * notification callbacks a Can driver is defined to call (section 7.1
 * rule 9, section 7.2's sequence diagram). That is a routing/notification
 * contract, not application logic, so it does not require this driver to
 * know COM signals. The two calls are made from Can.c (which includes
 * CanIf.h), not from this header, so Can.h itself has no CanIf
 * dependency. On the real ECU binaries the CanIf symbols resolve at final
 * executable link time (same header-only-dependency seam already used for
 * Mcu/Port/Gpt -> Det; see CMakeLists.txt's comment on bsw_mcal's include
 * dirs). Unit tests link a fake CanIf instead (tests/fixtures/CanIf_Fake.c)
 * so this driver is tested in isolation, without pulling in the real
 * CanIf.c.
 *
 * Deviation: a real microcontroller Can driver exposes hardware object
 * handles (HTH for transmit, HRH for receive) tied to real mailboxes. This
 * project has exactly one controller per ECU and one raw socket per
 * controller, so `Can_HwHandleType` (the `hth` parameter of `Can_Write`)
 * is simply the controller ID, and the `Hoh` field this driver reports in
 * `Can_HwType` on receive is always 0 (the one and only receive object).
 */
#ifndef CAN_H
#define CAN_H

#include "ComStack_Types.h"
#include "Platform_Types.h"
#include "Std_Types.h"

/** Real AUTOSAR `Can_ControllerStateType` values (section 9.2/9.3
 * reference `CAN_CS_STARTED`/`CAN_CS_STOPPED` by name; `CAN_CS_SLEEP` is
 * declared for API completeness per section 7.4 but nothing in this
 * project ever requests it). Declared here, not ComStack_Types.h, since
 * section 7.3 did not enumerate it and it is Can/CanIf-specific. */
typedef enum {
    CAN_CS_UNINIT = 0,
    CAN_CS_STOPPED = 1,
    CAN_CS_STARTED = 2,
    CAN_CS_SLEEP = 3
} Can_ControllerStateType;

typedef struct {
    uint8 controllerId;
    const char *interfaceName; /* e.g. "vcan0"; post-build-like per section 11.1 */
    boolean nonBlocking;       /* SHALL be TRUE (section 8.1: no periodic task may block on
                                * CAN); kept as an explicit, validated field rather than
                                * assumed, so a misconfiguration is a caught DET error
                                * instead of silent UB. */
} Can_ControllerConfigType;

typedef struct {
    const Can_ControllerConfigType *controllers;
    uint8 numControllers;
} Can_ConfigType;

/** Section 6.1: every I-PDU in this project uses fixed DLC 8. The driver
 * itself is generic and only enforces the classic-CAN hardware ceiling of
 * 8 bytes; the fixed-8 project convention is CanIf/COM's job to enforce. */
#define CAN_CLASSIC_MAX_DLC ((uint8)8U)

void Can_Init(const Can_ConfigType *config);
void Can_DeInit(void);

Std_ReturnType Can_SetControllerMode(uint8 controller_id, Can_ControllerStateType mode);

/** Test/inspection surface, and also CanIf's live transmit gate (section
 * 3.1: CanIf owns "controller state" together with Can; CanIf reads it
 * here instead of caching its own copy, so the two modules can never
 * disagree). Returns CAN_CS_UNINIT for an out-of-range or never-configured
 * controller_id. */
Can_ControllerStateType Can_GetControllerMode(uint8 controller_id);

/** REQ-ECUM-001: unlike a virtual hardware link (REQ-ECUM-005), a CAN
 * interface that never opened SHALL be a startup failure. EcuM (Phase 6)
 * checks this after Can_Init and aborts startup if any configured
 * controller is not ready; this driver itself never crashes or blocks on
 * the failure. */
boolean Can_IsControllerReady(uint8 controller_id);

Std_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu_info);

/**
 * Section 7.2 rule 1: drains all currently available matching frames
 * without blocking. Bounded per controller per call (like VirtualHw's
 * REQ-VHW-002 sibling rule) so one call can never starve other tasks.
 * Frames received while a controller is not CAN_CS_STARTED are drained
 * from the kernel socket buffer (so it cannot silently fill up) but are
 * dropped without notifying CanIf, matching a real bus-off/stopped
 * controller producing no Rx indications.
 */
void Can_MainFunction_Read(void);

/** Reserved: section 7.4 requires the API to exist. On this nonblocking
 * SocketCAN transport, Can_Write already reports success/failure
 * synchronously (a write() either queues immediately or fails with
 * EAGAIN) -- there is no deferred completion to poll here yet. Same
 * documented-reserved-no-op pattern as Dem_MainFunction. */
void Can_MainFunction_Write(void);

/* Test/inspection surface (section 12.1: not an alternate data path). */
uint32 Can_GetRxFrameCount(uint8 controller_id);
uint32 Can_GetRxDroppedCount(uint8 controller_id);
uint32 Can_GetTxFailureCount(uint8 controller_id);

#endif /* CAN_H */
