/**
 * @file test_virtual_hw.c
 * @brief Phase 2 unit tests for VirtualHw (bsw/mcal/VirtualHw).
 *
 * Covers section 14.1 ("test_virtual_hw"): bounded drain, latest datagram
 * wins, malformed and oversized datagram rejected, send with no peer is
 * non-fatal. Also covers REQ-VHW-004 (duplicate/reordered SEQ rejection).
 *
 * Uses REAL AF_UNIX sockets (this is the layer that owns them) with the
 * REAL Gpt (real clock) — none of these assertions need controlled time,
 * only real socket behavior. A raw peer socket created directly in this
 * file plays the role of the HMI/scenario player sending datagrams; this
 * is legitimate test instrumentation (spec section 12.1) and is never
 * linked into a production ECU target.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Det.h"
#include "Platform_Types.h"
#include "VirtualHw.h"

#define RX_SOCKET_PATH "/tmp/autosar_test_vhw_rx.sock"
#define TX_SOCKET_PATH "/tmp/autosar_test_vhw_tx_no_listener.sock"

#define LINK_RX 0U
#define LINK_TX 1U

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

/* A raw, unbound AF_UNIX SOCK_DGRAM socket used only to inject datagrams
 * into the RX link, playing the role of the HMI/scenario player. */
static int OpenPeerSender(void) {
    return socket(AF_UNIX, SOCK_DGRAM, 0);
}

static void SendBytes(int fd, const char *path, const char *data, size_t length) {
    struct sockaddr_un addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1U);
    (void)sendto(fd, data, length, 0, (const struct sockaddr *)&addr, sizeof(addr));
}

static void SendText(int fd, const char *path, const char *payload) {
    SendBytes(fd, path, payload, strlen(payload));
}

static const VirtualHw_LinkConfigType g_links[2] = {
    {LINK_RX, VIRTUALHW_DIR_RX, RX_SOCKET_PATH, 8U, 500U},
    {LINK_TX, VIRTUALHW_DIR_TX, TX_SOCKET_PATH, 0U, 0U},
};
static const VirtualHw_ConfigType g_config = {g_links, 2U};

static void TestBoundedDrainAndLatestWins(void) {
    (void)unlink(RX_SOCKET_PATH);
    Det_Init();
    VirtualHw_Init(&g_config);
    int peer = OpenPeerSender();
    Check(peer >= 0, "test peer socket created");

    /* Send 12 datagrams; the first activation should drain at most 8
     * (REQ-VHW-002) and cache the 8th one sent (0-indexed SEQ=7). */
    for (uint32 i = 0U; i < 12U; i++) {
        char msg[64];
        (void)snprintf(msg, sizeof(msg), "ADC0=%u;DIO0=1;SEQ=%u", (unsigned)(100U + i),
                       (unsigned)i);
        SendText(peer, RX_SOCKET_PATH, msg);
    }

    VirtualHw_MainFunction_Read(LINK_RX);
    Check(VirtualHw_GetRxValidCount(LINK_RX) == 8U,
          "first activation drains at most 8 datagrams (REQ-VHW-002)");

    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType age;
    boolean fresh;
    boolean hasLatest = VirtualHw_GetLatestPayload(LINK_RX, payload, sizeof(payload), &age, &fresh);
    Check(hasLatest, "a payload is cached after draining");
    uint32 seq = 0xFFFFFFFFU;
    (void)VirtualHw_ParseUint(payload, "SEQ", &seq);
    Check(seq == 7U,
          "the cached payload is the 8th (last) one drained this activation (latest wins)");

    VirtualHw_MainFunction_Read(LINK_RX);
    Check(VirtualHw_GetRxValidCount(LINK_RX) == 12U,
          "a second activation drains the remaining 4 datagrams");
    (void)VirtualHw_GetLatestPayload(LINK_RX, payload, sizeof(payload), &age, &fresh);
    (void)VirtualHw_ParseUint(payload, "SEQ", &seq);
    Check(seq == 11U, "the cache now holds the very last datagram sent");

    (void)close(peer);
    VirtualHw_DeInit();
}

static void TestMalformedAndOversizedRejected(void) {
    (void)unlink(RX_SOCKET_PATH);
    Det_Init();
    VirtualHw_Init(&g_config);
    int peer = OpenPeerSender();

    SendText(peer, RX_SOCKET_PATH, "ADC0=500;DIO0=1;SEQ=1");
    VirtualHw_MainFunction_Read(LINK_RX);
    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType age;
    boolean fresh;
    (void)VirtualHw_GetLatestPayload(LINK_RX, payload, sizeof(payload), &age, &fresh);
    uint32 goodValue = 0U;
    (void)VirtualHw_ParseUint(payload, "ADC0", &goodValue);
    Check(goodValue == 500U, "baseline valid datagram is accepted");

    uint32 malformedBefore = VirtualHw_GetRxMalformedCount(LINK_RX);

    /* No '=' anywhere: fails the generic well-formed check. */
    SendBytes(peer, RX_SOCKET_PATH, "not-a-key-value-message", 23U);

    /* Oversized: a valid-looking "key=value;..." message padded past the
     * 96-byte cap, so length — not shape — is what triggers rejection. */
    char oversized[130];
    (void)snprintf(oversized, sizeof(oversized), "ADC0=1;DIO0=1;SEQ=1;PAD=");
    size_t prefixLen = strlen(oversized);
    (void)memset(&oversized[prefixLen], 'X', sizeof(oversized) - prefixLen - 1U);
    oversized[sizeof(oversized) - 1U] = '\0';
    SendBytes(peer, RX_SOCKET_PATH, oversized, strlen(oversized));

    VirtualHw_MainFunction_Read(LINK_RX);

    Check(VirtualHw_GetRxMalformedCount(LINK_RX) == malformedBefore + 2U,
          "both the unparseable and the oversized datagram are counted as malformed (REQ-VHW-003)");

    char payloadAfter[VIRTUALHW_MAX_PAYLOAD + 1U];
    (void)VirtualHw_GetLatestPayload(LINK_RX, payloadAfter, sizeof(payloadAfter), &age, &fresh);
    uint32 valueAfter = 0U;
    (void)VirtualHw_ParseUint(payloadAfter, "ADC0", &valueAfter);
    Check(valueAfter == 500U,
          "malformed/oversized datagrams do not refresh the cached payload (REQ-VHW-003)");

    (void)close(peer);
    VirtualHw_DeInit();
}

static void TestDuplicateAndReorderedSeqRejected(void) {
    (void)unlink(RX_SOCKET_PATH);
    Det_Init();
    VirtualHw_Init(&g_config);
    int peer = OpenPeerSender();

    SendText(peer, RX_SOCKET_PATH, "ADC0=100;DIO0=0;SEQ=5");
    VirtualHw_MainFunction_Read(LINK_RX);

    uint32 duplicateBefore = VirtualHw_GetRxDuplicateCount(LINK_RX);

    /* SEQ=3 is older than the last-accepted SEQ=5: must be rejected. */
    SendText(peer, RX_SOCKET_PATH, "ADC0=999;DIO0=1;SEQ=3");
    VirtualHw_MainFunction_Read(LINK_RX);

    Check(VirtualHw_GetRxDuplicateCount(LINK_RX) == duplicateBefore + 1U,
          "an older SEQ is rejected as duplicate/reordered (REQ-VHW-004)");

    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType age;
    boolean fresh;
    (void)VirtualHw_GetLatestPayload(LINK_RX, payload, sizeof(payload), &age, &fresh);
    uint32 value = 0U;
    (void)VirtualHw_ParseUint(payload, "ADC0", &value);
    Check(value == 100U, "the older, rejected datagram never overwrites the cached payload");

    (void)close(peer);
    VirtualHw_DeInit();
}

static void TestSendWithNoPeerIsNonFatal(void) {
    (void)unlink(TX_SOCKET_PATH); /* make sure nobody is listening */
    Det_Init();
    VirtualHw_Init(&g_config);

    uint32 failuresBefore = VirtualHw_GetTxFailureCount(LINK_TX);
    Std_ReturnType r = VirtualHw_Send(LINK_TX, "STATE=OFF;SPEED=0;FLAGS=0x00;T=0", 33U);
    Check(r == E_NOT_OK, "sending with no listener returns E_NOT_OK, not a crash");
    Check(VirtualHw_GetTxFailureCount(LINK_TX) == failuresBefore + 1U,
          "the failed send is counted");

    /* Reaching this line at all proves VirtualHw_Send returned promptly
     * instead of blocking (ctest's own timeout is the ultimate backstop). */
    Check(1, "VirtualHw_Send returned without blocking");

    VirtualHw_DeInit();
}

int main(void) {
    TestBoundedDrainAndLatestWins();
    TestMalformedAndOversizedRejected();
    TestDuplicateAndReorderedSeqRejected();
    TestSendWithNoPeerIsNonFatal();

    (void)unlink(RX_SOCKET_PATH);

    if (g_failures == 0) {
        (void)printf("test_virtual_hw: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_virtual_hw: %d check(s) failed\n", g_failures);
    return 1;
}
