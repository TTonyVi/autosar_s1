/**
 * @file test_com_protection.c
 * @brief Phase 4 unit tests for ComProtection (bsw/communication/Com).
 *
 * Covers section 14.1 ("test_com_protection"): the CRC8 algorithm against
 * all three of section 6.5's golden vectors, and the alive-counter
 * acceptance rule. Pure logic, no Det/Gpt/Com dependency at all.
 */
#include <stdio.h>

#include "ComProtection.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void TestGoldenVectorsCrc8(void) {
    uint8 p1[8] = {0xD2U, 0x04U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    Check(ComProtection_ComputeCrc8(0x0180U, p1, 4U) == 0x19U,
          "golden vector 1: PduID 0x180 -> CRC 0x19");

    uint8 p2[8] = {0x01U, 0xD2U, 0x04U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    Check(ComProtection_ComputeCrc8(0x0181U, p2, 5U) == 0xB0U,
          "golden vector 2: PduID 0x181 -> CRC 0xB0");

    uint8 p3[8] = {0x10U, 0x27U, 0x01U, 0x0FU, 0x00U, 0x00U, 0x00U, 0x00U};
    Check(ComProtection_ComputeCrc8(0x0180U, p3, 4U) == 0xB1U,
          "golden vector 3 (overspeed threshold, max alive counter): PduID 0x180 -> CRC 0xB1");
}

static void TestCrcByteOwnPriorContentIsIgnored(void) {
    uint8 a[8] = {0xD2U, 0x04U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    uint8 b[8] = {0xD2U, 0x04U, 0x01U, 0x00U, 0xAAU, 0x00U, 0x00U, 0x00U}; /* garbage at index 4 */
    Check(ComProtection_ComputeCrc8(0x0180U, a, 4U) == ComProtection_ComputeCrc8(0x0180U, b, 4U),
          "the byte at crcByteIndex is always treated as zero, regardless of its prior content");
}

static void TestCounterValidity(void) {
    Check(ComProtection_IsCounterValid(FALSE, 0U, 7U),
          "the first-ever received counter is always accepted, whatever its value");
    Check(ComProtection_IsCounterValid(TRUE, 5U, 6U), "6 correctly follows 5");
    Check(!ComProtection_IsCounterValid(TRUE, 5U, 7U), "7 does not follow 5 (a skipped frame)");
    Check(!ComProtection_IsCounterValid(TRUE, 5U, 5U),
          "a repeated counter is rejected as duplicate");
    Check(ComProtection_IsCounterValid(TRUE, 15U, 0U),
          "the counter wraps from 15 to 0 (modulo 16)");
}

int main(void) {
    TestGoldenVectorsCrc8();
    TestCrcByteOwnPriorContentIsIgnored();
    TestCounterValidity();

    if (g_failures == 0) {
        (void)printf("test_com_protection: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_com_protection: %d check(s) failed\n", g_failures);
    return 1;
}
