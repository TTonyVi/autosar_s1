/**
 * @file test_canif.c
 * @brief Phase 3/4 unit tests for CanIf (bsw/communication/CanIf).
 *
 * Covers section 14.1 ("test_canif") and the Phase 3 exit criteria
 * (section 16): wrong ID/DLC/controller-state cases on both the transmit
 * and receive paths, plus section 11.3's config validation
 * (REQ-CFG-001/003). Uses the REAL CanIf.c against a FAKE Can
 * (tests/fixtures/Can_Fake.c) and, since Phase 4, a FAKE PduR
 * (tests/fixtures/PduR_Fake.c) to verify CanIf forwards an accepted
 * receive to `PduR_CanIfRxIndication` and a transmit's completion to
 * `PduR_CanIfTxConfirmation` -- without needing a real SocketCAN
 * interface (that is test_can's job, against the real Can.c).
 */
#include <stdio.h>
#include <string.h>

#include "CanIf.h"
#include "Can_Fake.h"
#include "Det.h"
#include "PduR_Fake.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

#define CONTROLLER_ID 0U

static const CanIf_TxPduConfigType g_txPdus[1] = {
    {0U, 0x180U, 8U, (Can_HwHandleType)CONTROLLER_ID, CONTROLLER_ID},
};
static const CanIf_RxPduConfigType g_rxPdus[1] = {
    {0U, 0x181U, 8U, CONTROLLER_ID},
};
static const CanIf_ConfigType g_config = {g_txPdus, 1U, g_rxPdus, 1U};

static void TestTransmitSuccessCallsCanWrite(void) {
    Det_Init();
    CanFake_Reset();
    PduRFake_Reset();
    CanIf_Init(&g_config);
    CanFake_SetControllerModeDirect(CONTROLLER_ID, CAN_CS_STARTED);

    uint8 data[8] = {0xD2U, 0x04U, 0x01U, 0x00U, 0x19U, 0x00U, 0x00U, 0x00U};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    Std_ReturnType r = CanIf_Transmit(0U, &info);
    Check(r == E_OK, "CanIf_Transmit succeeds when the controller is started and DLC matches");
    Check(CanFake_GetWriteCount() == 1U, "Can_Write was called exactly once");

    CanFake_WriteRecordType rec;
    Check(CanFake_GetLastWrite(&rec), "a write was recorded");
    Check(rec.id == 0x180U, "the physical CAN ID from config is used, not the logical PDU id");
    Check(rec.length == 8U, "the DLC matches");
    Check(memcmp(rec.data, data, 8U) == 0, "the payload bytes are forwarded verbatim");

    CanIf_DeInit();
}

static void TestTxConfirmationForwardsToPduR(void) {
    Det_Init();
    CanFake_Reset();
    PduRFake_Reset();
    CanIf_Init(&g_config);

    CanIf_TxConfirmation(0U);
    Check(PduRFake_GetCanIfTxConfirmationCount() == 1U,
          "a successful CanIf_TxConfirmation forwards to PduR_CanIfTxConfirmation (section 7.1 "
          "rule 9)");
    PduIdType confirmedId = 0xFFFFU;
    Check(PduRFake_GetLastCanIfTxConfirmationId(&confirmedId), "a confirmation was recorded");
    Check(confirmedId == 0U, "the CanIf-side tx pdu id is forwarded unchanged");

    CanIf_DeInit();
}

static void TestTransmitWrongDlcRejected(void) {
    Det_Init();
    CanFake_Reset();
    CanIf_Init(&g_config);
    CanFake_SetControllerModeDirect(CONTROLLER_ID, CAN_CS_STARTED);

    uint8 data[4] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 4U; /* configured dlc is 8 */

    Std_ReturnType r = CanIf_Transmit(0U, &info);
    Check(r == E_NOT_OK,
          "a Tx PDU with the wrong length is rejected before Can_Write is ever called");
    Check(CanFake_GetWriteCount() == 0U, "Can_Write was never called");

    CanIf_DeInit();
}

static void TestTransmitUnmappedPduIdRejected(void) {
    Det_Init();
    CanFake_Reset();
    CanIf_Init(&g_config);
    CanFake_SetControllerModeDirect(CONTROLLER_ID, CAN_CS_STARTED);

    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    Std_ReturnType r = CanIf_Transmit(99U, &info); /* only PDU 0 is configured */
    Check(r == E_NOT_OK, "an unmapped/out-of-range logical Tx PDU ID is rejected");
    Check(CanFake_GetWriteCount() == 0U, "Can_Write was never called");

    CanIf_DeInit();
}

static void TestTransmitControllerNotStartedRejected(void) {
    Det_Init();
    CanFake_Reset();
    CanIf_Init(&g_config);
    /* leave the fake controller at its default (not started) */

    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    Std_ReturnType r = CanIf_Transmit(0U, &info);
    Check(r == E_NOT_OK, "transmit is rejected while the controller is not CAN_CS_STARTED");
    Check(CanFake_GetWriteCount() == 0U, "Can_Write was never called");

    CanIf_DeInit();
}

static void TestRxIndicationAcceptsMappedIdAndDlc(void) {
    Det_Init();
    CanFake_Reset();
    PduRFake_Reset();
    CanIf_Init(&g_config);

    Can_HwType mailbox;
    mailbox.CanId = 0x181U;
    mailbox.Hoh = 0U;
    mailbox.ControllerId = CONTROLLER_ID;
    uint8 data[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    uint32 acceptedBefore = CanIf_GetRxAcceptedCount();
    CanIf_RxIndication(&mailbox, &info);
    Check(CanIf_GetRxAcceptedCount() == acceptedBefore + 1U,
          "a mapped CAN ID/DLC pair is accepted");

    PduIdType lastId = 99U;
    Check(CanIf_GetLastRxPduId(&lastId) == E_OK, "the accepted PDU id is retrievable");
    Check(lastId == 0U, "it matches the configured canIfRxPduId");

    Check(PduRFake_GetCanIfRxIndicationCount() == 1U,
          "an accepted frame forwards to PduR_CanIfRxIndication (section 7.2)");
    PduIdType forwardedId = 0xFFFFU;
    Check(PduRFake_GetLastCanIfRxIndicationId(&forwardedId), "a forward was recorded");
    Check(forwardedId == 0U, "the configured canIfRxPduId is forwarded");

    CanIf_DeInit();
}

static void TestRxIndicationRejectsUnmappedCanId(void) {
    Det_Init();
    CanFake_Reset();
    PduRFake_Reset();
    CanIf_Init(&g_config);

    Can_HwType mailbox;
    mailbox.CanId = 0x7FFU; /* not configured */
    mailbox.Hoh = 0U;
    mailbox.ControllerId = CONTROLLER_ID;
    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    uint32 rejectedBefore = CanIf_GetRxRejectedCount();
    CanIf_RxIndication(&mailbox, &info);
    Check(CanIf_GetRxRejectedCount() == rejectedBefore + 1U,
          "an unmapped CAN ID is rejected (wrong-ID case)");
    Check(PduRFake_GetCanIfRxIndicationCount() == 0U,
          "a rejected frame is never forwarded to PduR");

    CanIf_DeInit();
}

static void TestRxIndicationRejectsWrongDlc(void) {
    Det_Init();
    CanFake_Reset();
    PduRFake_Reset();
    CanIf_Init(&g_config);

    Can_HwType mailbox;
    mailbox.CanId = 0x181U;
    mailbox.Hoh = 0U;
    mailbox.ControllerId = CONTROLLER_ID;
    uint8 data[4] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 4U; /* configured dlc is 8 */

    uint32 rejectedBefore = CanIf_GetRxRejectedCount();
    CanIf_RxIndication(&mailbox, &info);
    Check(CanIf_GetRxRejectedCount() == rejectedBefore + 1U,
          "a mapped CAN ID with the wrong DLC is rejected");

    PduIdType lastId = 0U;
    Check(CanIf_GetLastRxPduId(&lastId) == E_NOT_OK,
          "no accepted PDU id is recorded when every RxIndication so far was rejected");
    Check(PduRFake_GetCanIfRxIndicationCount() == 0U,
          "a rejected frame is never forwarded to PduR");

    CanIf_DeInit();
}

static const CanIf_RxPduConfigType g_dupRxPdus[2] = {
    {0U, 0x181U, 8U, CONTROLLER_ID},
    {1U, 0x181U, 8U, CONTROLLER_ID}, /* duplicate physical CAN ID + controller */
};
static const CanIf_ConfigType g_dupConfig = {g_txPdus, 1U, g_dupRxPdus, 2U};

static void TestDuplicateRxCanIdRejected(void) {
    Det_Init();
    CanFake_Reset();
    CanIf_Init(&g_dupConfig);
    CanFake_SetControllerModeDirect(CONTROLLER_ID, CAN_CS_STARTED);

    /* A duplicate physical CAN ID in the Rx table (REQ-CFG-003) SHALL fail
     * the whole Init; verify indirectly, since CanIf exposes no
     * "IsInitialized" getter -- an otherwise-valid transmit must now fail
     * because CanIf never became initialized. */
    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;
    Std_ReturnType r = CanIf_Transmit(0U, &info);
    Check(r == E_NOT_OK, "CanIf_Init rejects a duplicate physical CAN ID in the Rx table, leaving "
                         "CanIf uninitialized");

    CanIf_DeInit();
}

static void TestUninitializedCallsRejected(void) {
    CanIf_DeInit();
    Det_Init();
    CanFake_Reset();
    PduRFake_Reset();
    /* No CanIf_Init call at all. */

    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;
    Check(CanIf_Transmit(0U, &info) == E_NOT_OK,
          "CanIf_Transmit before Init is rejected (REQ-ECUM-002)");
    Check(CanIf_SetControllerMode(CONTROLLER_ID, CAN_CS_STARTED) == E_NOT_OK,
          "CanIf_SetControllerMode before Init is rejected");

    Can_HwType mailbox;
    mailbox.CanId = 0x181U;
    mailbox.Hoh = 0U;
    mailbox.ControllerId = CONTROLLER_ID;
    CanIf_RxIndication(&mailbox, &info); /* must not crash */
    Check(CanIf_GetRxAcceptedCount() == 0U, "an RxIndication before Init is not accepted");
    Check(PduRFake_GetCanIfRxIndicationCount() == 0U, "nothing is forwarded to PduR before Init");

    CanIf_TxConfirmation(0U); /* must not crash */
    Check(PduRFake_GetCanIfTxConfirmationCount() == 0U,
          "a TxConfirmation before Init is not forwarded to PduR");
}

int main(void) {
    TestTransmitSuccessCallsCanWrite();
    TestTxConfirmationForwardsToPduR();
    TestTransmitWrongDlcRejected();
    TestTransmitUnmappedPduIdRejected();
    TestTransmitControllerNotStartedRejected();
    TestRxIndicationAcceptsMappedIdAndDlc();
    TestRxIndicationRejectsUnmappedCanId();
    TestRxIndicationRejectsWrongDlc();
    TestDuplicateRxCanIdRejected();
    TestUninitializedCallsRejected();

    if (g_failures == 0) {
        (void)printf("test_canif: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_canif: %d check(s) failed\n", g_failures);
    return 1;
}
