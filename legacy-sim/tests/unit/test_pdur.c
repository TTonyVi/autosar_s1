/**
 * @file test_pdur.c
 * @brief Phase 4 unit tests for PduR (bsw/communication/PduR).
 *
 * Covers section 14.1 ("test_pdur"). Uses the REAL PduR.c against a FAKE
 * CanIf (tests/fixtures/CanIf_Fake.c, its CanIf_Transmit role) and a FAKE
 * Com (tests/fixtures/Com_Fake.c), so PduR's own id-only routing logic is
 * tested deterministically without a real CAN transport or signal
 * configuration.
 */
#include <stdio.h>
#include <string.h>

#include "CanIf_Fake.h"
#include "Com_Fake.h"
#include "Det.h"
#include "PduR.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

#define COM_TX_IPDU_ID 0U
#define CANIF_TX_PDU_ID 0U
#define CANIF_RX_PDU_ID 0U
#define COM_RX_IPDU_ID 0U

static const PduR_RouteType g_routes[2] = {
    {PDUR_MODULE_COM, COM_TX_IPDU_ID, PDUR_MODULE_CANIF, CANIF_TX_PDU_ID}, /* Tx route */
    {PDUR_MODULE_CANIF, CANIF_RX_PDU_ID, PDUR_MODULE_COM, COM_RX_IPDU_ID}, /* Rx route */
};
static const PduR_ConfigType g_config = {g_routes, 2U};

static void SetUp(void) {
    Det_Init();
    CanIfFake_Reset();
    ComFake_Reset();
    PduR_Init(&g_config);
}

static void TestComTransmitForwardsToCanIfTransmit(void) {
    SetUp();
    uint8 data[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    Std_ReturnType r = PduR_ComTransmit(COM_TX_IPDU_ID, &info);
    Check(r == E_OK, "PduR_ComTransmit returns CanIf_Transmit's result (E_OK here)");
    Check(CanIfFake_GetTransmitCount() == 1U, "CanIf_Transmit was called exactly once");

    PduIdType id = 0xFFFFU;
    Check(CanIfFake_GetLastTransmitId(&id), "a transmit was recorded");
    Check(id == CANIF_TX_PDU_ID, "the resolved CanIf-side id from the route is used");
    uint8 rxData[8];
    uint8 rxLen = 0U;
    Check(CanIfFake_GetLastTransmitData(rxData, sizeof(rxData), &rxLen), "payload was recorded");
    Check(rxLen == 8U, "length matches");
    Check(memcmp(rxData, data, 8U) == 0, "the payload is forwarded unchanged (REQ-ARCH-003)");
}

static void TestComTransmitReturnsCanIfFailure(void) {
    SetUp();
    CanIfFake_SetTransmitResult(E_NOT_OK);
    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    Std_ReturnType r = PduR_ComTransmit(COM_TX_IPDU_ID, &info);
    Check(r == E_NOT_OK, "a downstream CanIf_Transmit failure is forwarded, not swallowed");
}

static void TestComTransmitUnmappedIdRejected(void) {
    SetUp();
    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    Std_ReturnType r = PduR_ComTransmit(99U, &info);
    Check(r == E_NOT_OK, "an unmapped COM-side id is rejected");
    Check(CanIfFake_GetTransmitCount() == 0U, "CanIf_Transmit is never called for an unmapped id");
}

static void TestCanIfRxIndicationForwardsToComRxIndication(void) {
    SetUp();
    uint8 data[8] = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    PduR_CanIfRxIndication(CANIF_RX_PDU_ID, &info);
    Check(ComFake_GetRxIndicationCount() == 1U, "Com_RxIndication was called exactly once");
    PduIdType id = 0xFFFFU;
    Check(ComFake_GetLastRxIndicationId(&id), "a receive was recorded");
    Check(id == COM_RX_IPDU_ID, "the resolved COM-side id from the route is used");
    uint8 rxData[8];
    uint8 rxLen = 0U;
    Check(ComFake_GetLastRxIndicationData(rxData, sizeof(rxData), &rxLen), "payload was recorded");
    Check(memcmp(rxData, data, 8U) == 0, "the payload is forwarded unchanged (REQ-ARCH-003)");
}

static void TestCanIfRxIndicationUnmappedIdRejected(void) {
    SetUp();
    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    PduR_CanIfRxIndication(99U, &info);
    Check(ComFake_GetRxIndicationCount() == 0U,
          "Com_RxIndication is never called for an unmapped CanIf-side id");
}

static void TestCanIfTxConfirmationForwardsToComTxConfirmationViaReverseLookup(void) {
    SetUp();
    PduR_CanIfTxConfirmation(CANIF_TX_PDU_ID);
    Check(ComFake_GetTxConfirmationCount() == 1U, "Com_TxConfirmation was called exactly once");
    PduIdType id = 0xFFFFU;
    Check(ComFake_GetLastTxConfirmationId(&id), "a confirmation was recorded");
    Check(id == COM_TX_IPDU_ID,
          "the reverse lookup on the Tx route resolves back to the COM-side id");
}

static void TestCanIfTxConfirmationUnmappedIdRejected(void) {
    SetUp();
    PduR_CanIfTxConfirmation(99U);
    Check(ComFake_GetTxConfirmationCount() == 0U,
          "Com_TxConfirmation is never called for an unmapped CanIf-side id");
}

static const PduR_RouteType g_duplicateRoutes[2] = {
    {PDUR_MODULE_COM, COM_TX_IPDU_ID, PDUR_MODULE_CANIF, CANIF_TX_PDU_ID},
    {PDUR_MODULE_COM, COM_TX_IPDU_ID, PDUR_MODULE_CANIF, (PduIdType)(CANIF_TX_PDU_ID + 1U)},
};
static const PduR_ConfigType g_duplicateConfig = {g_duplicateRoutes, 2U};

static void TestDuplicateRouteSourceRejectedAtInit(void) {
    Det_Init();
    CanIfFake_Reset();
    ComFake_Reset();
    PduR_Init(&g_duplicateConfig);

    /* Init should have refused the whole config; verify indirectly since
     * PduR exposes no "IsInitialized" getter -- an otherwise-valid
     * transmit must now fail because PduR never became initialized. */
    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;
    Std_ReturnType r = PduR_ComTransmit(COM_TX_IPDU_ID, &info);
    Check(r == E_NOT_OK,
          "PduR_Init rejects a duplicate (sourceModule, sourcePduId) pair, leaving PduR "
          "uninitialized");
}

static void TestCallsBeforeInitRejected(void) {
    Det_Init();
    CanIfFake_Reset();
    ComFake_Reset();
    PduR_DeInit(); /* ensure a clean, uninitialized state regardless of test order */

    uint8 data[8] = {0};
    PduInfoType info;
    info.SduDataPtr = data;
    info.SduLength = 8U;

    Check(PduR_ComTransmit(COM_TX_IPDU_ID, &info) == E_NOT_OK,
          "PduR_ComTransmit before Init is rejected (REQ-ECUM-002)");
    PduR_CanIfRxIndication(CANIF_RX_PDU_ID, &info); /* must not crash */
    Check(ComFake_GetRxIndicationCount() == 0U, "no forwarding happens before Init");
    PduR_CanIfTxConfirmation(CANIF_TX_PDU_ID); /* must not crash */
    Check(ComFake_GetTxConfirmationCount() == 0U, "no forwarding happens before Init");
}

int main(void) {
    TestComTransmitForwardsToCanIfTransmit();
    TestComTransmitReturnsCanIfFailure();
    TestComTransmitUnmappedIdRejected();
    TestCanIfRxIndicationForwardsToComRxIndication();
    TestCanIfRxIndicationUnmappedIdRejected();
    TestCanIfTxConfirmationForwardsToComTxConfirmationViaReverseLookup();
    TestCanIfTxConfirmationUnmappedIdRejected();
    TestDuplicateRouteSourceRejectedAtInit();
    TestCallsBeforeInitRejected();

    if (g_failures == 0) {
        (void)printf("test_pdur: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_pdur: %d check(s) failed\n", g_failures);
    return 1;
}
