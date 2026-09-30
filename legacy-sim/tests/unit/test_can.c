/**
 * @file test_can.c
 * @brief Phase 3 unit tests for Can (bsw/mcal/Can).
 *
 * Covers section 14.1 ("test_can") and the Phase 3 exit criteria (section
 * 16): a loopback test on the real "vcan0" interface, plus wrong
 * ID/DLC/controller-state cases. Uses the REAL Can.c (this is the layer
 * that owns the SocketCAN socket, so it must be proven against real
 * kernel behavior) with a FAKE CanIf (tests/fixtures/CanIf_Fake.c) so
 * assertions can inspect exactly what Can.c handed upward, without
 * depending on CanIf's own routing logic (that is test_canif's job).
 *
 * Prerequisite: "vcan0" must exist and be up (tools/setup_vcan.sh; see
 * README.md). SocketCAN's local loopback (CAN_RAW_LOOPBACK, on by
 * default) rebroadcasts a frame written by one local socket to every
 * OTHER local socket bound to the same interface -- but not back to the
 * sender itself (CAN_RAW_RECV_OWN_MSGS is off by default). That two-party
 * shape is exactly what lets the two separate SensorEcu/ClusterEcu
 * processes talk over vcan0 with no real bus hardware, so these tests
 * mirror it: a second raw CAN socket opened directly in this file plays
 * the role of "the other ECU" -- observing what Can_Write puts on the
 * bus, and independently injecting frames for Can_MainFunction_Read to
 * drain. This is legitimate test instrumentation (section 12.1) and is
 * never linked into a production ECU target.
 */
/* See Can.c for why both macros are needed (struct ifreq needs
 * _DEFAULT_SOURCE; sockets/fcntl need _POSIX_C_SOURCE). */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#ifndef PF_CAN
#define PF_CAN 29
#endif
#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

#include "Can.h"
#include "CanIf_Fake.h"
#include "Det.h"

#define TEST_CAN_INTERFACE "vcan0"
#define TEST_CONTROLLER_ID 0U

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static const Can_ControllerConfigType g_controllers[1] = {
    {TEST_CONTROLLER_ID, TEST_CAN_INTERFACE, TRUE},
};
static const Can_ConfigType g_config = {g_controllers, 1U};

/* An independent raw CAN socket, opened the same way Can.c opens its own,
 * used only to inject a frame from "outside" our Can driver instance. */
static int OpenPeerCanSocket(const char *interfaceName) {
    int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        return -1;
    }
    struct ifreq ifr;
    (void)memset(&ifr, 0, sizeof(ifr));
    (void)strncpy(ifr.ifr_name, interfaceName, sizeof(ifr.ifr_name) - 1U);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        (void)close(fd);
        return -1;
    }
    struct sockaddr_can addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

/**
 * Loopback test on vcan0 (Phase 3 exit criteria). vcan0's default
 * SocketCAN settings are CAN_RAW_LOOPBACK=1 (a frame written by any local
 * socket is rebroadcast to every OTHER local socket bound to the same
 * interface) and CAN_RAW_RECV_OWN_MSGS=0 (the sending socket itself does
 * NOT get its own frame back). That is exactly what makes vcan0 usable as
 * the "bus" between the two separate SensorEcu/ClusterEcu processes in
 * this project -- so this test deliberately mirrors that two-party shape
 * instead of expecting Can.c's own socket to see its own transmission:
 * an independent peer socket (playing the role of "the other ECU") is
 * what observes the write, and separately drives a receive back into our
 * Can_MainFunction_Read.
 */
static void TestTransmitIsObservedOnTheBus(void) {
    Det_Init();
    CanIfFake_Reset();
    Can_Init(&g_config);
    Check(Can_IsControllerReady(TEST_CONTROLLER_ID),
          "vcan0 controller opens and binds successfully");
    Check(Can_SetControllerMode(TEST_CONTROLLER_ID, CAN_CS_STARTED) == E_OK, "controller starts");

    int peer = OpenPeerCanSocket(TEST_CAN_INTERFACE);
    Check(peer >= 0, "an independent peer socket opens on vcan0 (plays the role of the other ECU)");

    uint8 data[8] = {0xD2U, 0x04U, 0x01U, 0x00U, 0x19U, 0x00U, 0x00U, 0x00U};
    Can_PduType pdu;
    pdu.swPduHandle = (PduIdType)42U;
    pdu.length = 8U;
    pdu.id = 0x180U;
    pdu.sdu = data;

    Std_ReturnType r = Can_Write((Can_HwHandleType)TEST_CONTROLLER_ID, &pdu);
    Check(r == E_OK, "Can_Write succeeds on a started controller");

    struct can_frame received;
    (void)memset(&received, 0, sizeof(received));
    ssize_t n = read(peer, &received, sizeof(received));
    Check(n == (ssize_t)sizeof(received),
          "the peer observes exactly one frame on the bus (loopback on vcan0)");
    Check((received.can_id & CAN_SFF_MASK) == 0x180U, "the CAN ID matches what was written");
    Check(received.can_dlc == 8U, "the DLC matches");
    Check(memcmp(received.data, data, 8U) == 0, "the payload bytes match, verbatim");

    Check(CanIfFake_GetTxConfirmationCount() == 1U,
          "a successful write triggers CanIf_TxConfirmation (section 7.1 rule 9)");
    PduIdType confirmedId = 0U;
    Check(CanIfFake_GetLastTxConfirmedPduId(&confirmedId), "a confirmation was recorded");
    Check(confirmedId == 42U, "the confirmed PDU id matches the one written");

    (void)close(peer);
    Can_DeInit();
}

static void TestOversizedWriteRejected(void) {
    Det_Init();
    CanIfFake_Reset();
    Can_Init(&g_config);
    (void)Can_SetControllerMode(TEST_CONTROLLER_ID, CAN_CS_STARTED);

    uint8 data[8] = {0};
    Can_PduType pdu;
    pdu.swPduHandle = 99U;
    pdu.length = 9U; /* > CAN_CLASSIC_MAX_DLC */
    pdu.id = 0x181U;
    pdu.sdu = data;

    uint32 failuresBefore = Can_GetTxFailureCount(TEST_CONTROLLER_ID);
    Std_ReturnType r = Can_Write((Can_HwHandleType)TEST_CONTROLLER_ID, &pdu);
    Check(r == E_NOT_OK, "a length greater than 8 is rejected before ever touching the socket");
    Check(Can_GetTxFailureCount(TEST_CONTROLLER_ID) == failuresBefore + 1U,
          "the failure is counted");
    Check(CanIfFake_GetTxConfirmationCount() == 0U, "no confirmation for a rejected write");

    Can_DeInit();
}

static void TestControllerNotStartedRejectsWrite(void) {
    Det_Init();
    CanIfFake_Reset();
    Can_Init(&g_config);
    /* Controller left in CAN_CS_STOPPED (Can_Init's default) -- never started. */

    uint8 data[8] = {0};
    Can_PduType pdu;
    pdu.swPduHandle = 1U;
    pdu.length = 8U;
    pdu.id = 0x180U;
    pdu.sdu = data;

    uint32 failuresBefore = Can_GetTxFailureCount(TEST_CONTROLLER_ID);
    Std_ReturnType r = Can_Write((Can_HwHandleType)TEST_CONTROLLER_ID, &pdu);
    Check(r == E_NOT_OK, "Can_Write is rejected while the controller is not CAN_CS_STARTED");
    Check(Can_GetTxFailureCount(TEST_CONTROLLER_ID) == failuresBefore + 1U,
          "the failure is counted");
    Check(CanIfFake_GetTxConfirmationCount() == 0U, "no confirmation for a rejected write");

    Can_DeInit();
}

static void TestFramesDroppedWhileStoppedAreNotIndicated(void) {
    Det_Init();
    CanIfFake_Reset();
    Can_Init(&g_config);
    /* leave stopped */

    int peer = OpenPeerCanSocket(TEST_CAN_INTERFACE);
    Check(peer >= 0, "an independent peer socket opens on vcan0");

    struct can_frame frame;
    (void)memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x181U;
    frame.can_dlc = 4U;
    frame.data[0] = 0xAAU;

    ssize_t n = write(peer, &frame, sizeof(frame));
    Check(n == (ssize_t)sizeof(frame), "the peer writes a frame directly onto vcan0");

    uint32 rxBefore = Can_GetRxFrameCount(TEST_CONTROLLER_ID);
    Can_MainFunction_Read();
    Check(Can_GetRxFrameCount(TEST_CONTROLLER_ID) == rxBefore + 1U,
          "the frame is drained from the kernel socket buffer even while stopped");
    Check(
        CanIfFake_GetRxIndicationCount() == 0U,
        "a stopped controller never forwards to CanIf_RxIndication (wrong controller-state case)");

    Check(Can_SetControllerMode(TEST_CONTROLLER_ID, CAN_CS_STARTED) == E_OK, "controller starts");
    n = write(peer, &frame, sizeof(frame));
    Check(n == (ssize_t)sizeof(frame), "the peer writes a second frame");
    Can_MainFunction_Read();
    Check(CanIfFake_GetRxIndicationCount() == 1U,
          "once started, the next frame is forwarded normally");

    (void)close(peer);
    Can_DeInit();
}

static const Can_ControllerConfigType g_badControllers[1] = {
    {TEST_CONTROLLER_ID, "vcan_does_not_exist_9999", TRUE},
};
static const Can_ConfigType g_badConfig = {g_badControllers, 1U};

static void TestUnopenableInterfaceBlocksStart(void) {
    Det_Init();
    Can_Init(&g_badConfig);
    Check(!Can_IsControllerReady(TEST_CONTROLLER_ID),
          "an interface that cannot be opened leaves the controller not-ready (REQ-ECUM-001)");
    Check(Can_GetControllerMode(TEST_CONTROLLER_ID) == CAN_CS_UNINIT,
          "its mode stays CAN_CS_UNINIT");
    Check(Can_SetControllerMode(TEST_CONTROLLER_ID, CAN_CS_STARTED) == E_NOT_OK,
          "it can never be started later just by asking");
    Can_DeInit();
}

static void TestInvalidWriteParamsRejected(void) {
    Det_Init();
    CanIfFake_Reset();
    Can_Init(&g_config);
    (void)Can_SetControllerMode(TEST_CONTROLLER_ID, CAN_CS_STARTED);

    Check(Can_Write((Can_HwHandleType)99U, NULL) == E_NOT_OK,
          "an out-of-range hth (bad controller selector) is rejected");
    Check(CanIfFake_GetTxConfirmationCount() == 0U, "nothing was confirmed");

    Can_DeInit();
}

int main(void) {
    TestTransmitIsObservedOnTheBus();
    TestOversizedWriteRejected();
    TestControllerNotStartedRejectsWrite();
    TestFramesDroppedWhileStoppedAreNotIndicated();
    TestUnopenableInterfaceBlocksStart();
    TestInvalidWriteParamsRejected();

    if (g_failures == 0) {
        (void)printf("test_can: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_can: %d check(s) failed\n", g_failures);
    return 1;
}
