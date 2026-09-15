/**
 * @file test_stub_mcal.c
 * @brief Proves REQ-ARCH-012: the stub MCAL variant works correctly on
 * its own terms AND every higher layer that consumes it needs zero
 * source changes to do so.
 *
 * Links the REAL Adc.c/Dio.c/CanIf.c/VirtualHw_Parse.c completely
 * unmodified, against the STUB Can_Stub.c/VirtualHw_Stub.c instead of
 * their real counterparts -- exactly the swap
 * `-DAUTOSAR_MCAL_VARIANT=stub` makes for the full ECU executables. If
 * this compiles and passes, Adc.c/Dio.c/CanIf.c (and by the same
 * reasoning Display.c, IoHwAb.c, and everything above it) provably do
 * not care which MCAL variant sits underneath them.
 *
 * `test_can`/`test_virtual_hw` remain the correctness tests for the REAL
 * socket-based drivers; this file is the correctness test for the STUB
 * drivers plus the substitution seam itself, not a replacement for either.
 */
#include <stdio.h>
#include <string.h>

#include "Adc.h"
#include "CanIf.h"
#include "Det.h"
#include "Dio.h"
#include "Gpt_Fake.h"
#include "VirtualHw_Stub.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

#define SENSOR_LINK_ID 0U

static void SetUpVirtualHw(void) {
    Det_Init();
    GptFake_Reset();
    Gpt_Init(NULL);

    static const VirtualHw_LinkConfigType links[1] = {
        {SENSOR_LINK_ID, VIRTUALHW_DIR_RX, NULL, 8U, 500U},
    };
    static const VirtualHw_ConfigType config = {links, 1U};
    VirtualHw_Init(&config);
}

static void TestVirtualHwStubRoundTrip(void) {
    SetUpVirtualHw();

    const char *payload = "ADC0=2048;DIO0=1;SEQ=1";
    VirtualHw_StubInject(SENSOR_LINK_ID, payload, (uint16)strlen(payload));
    VirtualHw_MainFunction_Read(SENSOR_LINK_ID);

    char buffer[128];
    Gpt_ValueType age = 0xFFFFFFFFU;
    boolean isFresh = FALSE;
    Check(VirtualHw_GetLatestPayload(SENSOR_LINK_ID, buffer, (uint16)sizeof(buffer), &age,
                                     &isFresh) == TRUE,
          "an injected datagram becomes the latest payload");
    Check(strcmp(buffer, payload) == 0, "the payload round-trips verbatim");
    Check(isFresh == TRUE, "a just-injected datagram is fresh");
    Check(VirtualHw_GetRxValidCount(SENSOR_LINK_ID) == 1U, "the valid-count counter advances");
}

static void TestVirtualHwStubRejectsMalformedAndDuplicate(void) {
    SetUpVirtualHw();

    VirtualHw_StubInject(SENSOR_LINK_ID, "not a kv payload", 16U);
    VirtualHw_MainFunction_Read(SENSOR_LINK_ID);
    Check(VirtualHw_GetRxMalformedCount(SENSOR_LINK_ID) == 1U,
          "a payload with no '=' is rejected as malformed, matching the real driver");

    const char *first = "ADC0=100;SEQ=5";
    VirtualHw_StubInject(SENSOR_LINK_ID, first, (uint16)strlen(first));
    VirtualHw_MainFunction_Read(SENSOR_LINK_ID);

    const char *stale = "ADC0=200;SEQ=5"; /* not newer */
    VirtualHw_StubInject(SENSOR_LINK_ID, stale, (uint16)strlen(stale));
    VirtualHw_MainFunction_Read(SENSOR_LINK_ID);
    Check(VirtualHw_GetRxDuplicateCount(SENSOR_LINK_ID) == 1U,
          "a non-newer SEQ is rejected, matching REQ-VHW-004");

    char buffer[128];
    (void)VirtualHw_GetLatestPayload(SENSOR_LINK_ID, buffer, (uint16)sizeof(buffer), NULL, NULL);
    Check(strcmp(buffer, first) == 0, "the stale duplicate never overwrote the accepted value");
}

static void TestAdcReadsThroughTheStubUnmodified(void) {
    SetUpVirtualHw();

    static const Adc_GroupConfigType groups[1] = {
        {0U, 0U, 12U, SENSOR_LINK_ID, 0U, 4095U},
    };
    static const Adc_ConfigType adcConfig = {groups, 1U};
    Adc_Init(&adcConfig);

    const char *payload = "ADC0=3000;SEQ=1";
    VirtualHw_StubInject(SENSOR_LINK_ID, payload, (uint16)strlen(payload));
    Adc_MainFunction_Read();

    Adc_ValueGroupType value = 0U;
    Check(Adc_ReadGroup(0U, &value) == E_OK, "Adc_ReadGroup succeeds against the stub, unmodified");
    Check(value == 3000U, "Adc_ReadGroup returns the injected raw count, unscaled (REQ-MCAL-001)");
}

static void TestDioReadsThroughTheStubUnmodified(void) {
    SetUpVirtualHw();

    static const Dio_ChannelConfigType channels[1] = {
        {0U, SENSOR_LINK_ID, STD_LOW},
    };
    static const Dio_ConfigType dioConfig = {channels, 1U};
    Dio_Init(&dioConfig);

    const char *payload = "DIO0=1;SEQ=1";
    VirtualHw_StubInject(SENSOR_LINK_ID, payload, (uint16)strlen(payload));
    Dio_MainFunction_Read();

    Check(Dio_ReadChannel(0U) == STD_HIGH, "Dio_ReadChannel reads the injected level, unmodified");
}

#define DISPLAY_LINK_ID 1U

static void TestVirtualHwStubSendRecordsForInspection(void) {
    Det_Init();
    static const VirtualHw_LinkConfigType links[1] = {
        {DISPLAY_LINK_ID, VIRTUALHW_DIR_TX, NULL, 0U, 0U},
    };
    static const VirtualHw_ConfigType config = {links, 1U};
    VirtualHw_Init(&config);

    const char *frame = "STATE=NORMAL;SPEED=1234;FLAGS=0x00;T=1";
    Check(VirtualHw_Send(DISPLAY_LINK_ID, frame, (uint16)strlen(frame)) == E_OK,
          "VirtualHw_Send succeeds against the stub");

    char buffer[128];
    Check(VirtualHw_StubGetLastSent(DISPLAY_LINK_ID, buffer, (uint16)sizeof(buffer)) == TRUE,
          "the sent frame was recorded");
    Check(strcmp(buffer, frame) == 0, "the recorded frame matches exactly, verbatim");
}

/* ------------------- Can_Stub + the real CanIf.c ---------------------- */

static const CanIf_TxPduConfigType kTxPdus[1] = {{0U, 0x180U, 8U, 0U, 0U}};
static const CanIf_RxPduConfigType kRxPdus[1] = {{0U, 0x180U, 8U, 0U}};
static const CanIf_ConfigType kCanIfConfig = {kTxPdus, 1U, kRxPdus, 1U};

static void TestCanStubLoopbackReachesCanIf(void) {
    Det_Init();
    CanIf_Init(&kCanIfConfig);

    static const Can_ControllerConfigType controllers[1] = {{0U, "unused-by-stub", TRUE}};
    static const Can_ConfigType canConfig = {controllers, 1U};
    Can_Init(&canConfig);
    Check(Can_IsControllerReady(0U) == TRUE, "the stub controller is ready with no real hardware");
    Check(Can_SetControllerMode(0U, CAN_CS_STARTED) == E_OK, "the stub controller starts");

    uint8 payload[8] = {0xAAU, 0xBBU, 0U, 0U, 0U, 0U, 0U, 0U};
    PduInfoType info;
    info.SduDataPtr = payload;
    info.SduLength = 8U;
    Check(CanIf_Transmit(0U, &info) == E_OK, "a transmit through CanIf succeeds against the stub");

    uint32 rxBefore = CanIf_GetRxAcceptedCount();
    Can_MainFunction_Read();
    Check(
        CanIf_GetRxAcceptedCount() == rxBefore + 1U,
        "the stub's own loopback delivers the frame back up through the REAL, unmodified CanIf.c");

    PduIdType lastId = 0xFFFFU;
    Check(CanIf_GetLastRxPduId(&lastId) == E_OK, "the accepted frame's PDU id is retrievable");
    Check(lastId == 0U, "the loopback frame maps to the same Rx PDU it was configured with");
}

int main(void) {
    TestVirtualHwStubRoundTrip();
    TestVirtualHwStubRejectsMalformedAndDuplicate();
    TestAdcReadsThroughTheStubUnmodified();
    TestDioReadsThroughTheStubUnmodified();
    TestVirtualHwStubSendRecordsForInspection();
    TestCanStubLoopbackReachesCanIf();

    if (g_failures != 0) {
        (void)fprintf(stderr, "test_stub_mcal: %d check(s) failed\n", g_failures);
        return 1;
    }
    (void)printf("test_stub_mcal: all checks passed\n");
    return 0;
}
