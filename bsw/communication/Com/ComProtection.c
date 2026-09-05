#include "ComProtection.h"

#include <string.h>

/* CRC-8/AUTOSAR: polynomial 0x1D, init 0xFF, no reflection, final XOR
 * 0xFF (section 6.5). Verified against all three of section 6.5's golden
 * vectors in test_com_protection. */
#define COMPROTECTION_CRC_POLY ((uint8)0x1DU)
#define COMPROTECTION_CRC_INIT ((uint8)0xFFU)
#define COMPROTECTION_CRC_XOROUT ((uint8)0xFFU)

static uint8 Crc8UpdateByte(uint8 crc, uint8 dataByte) {
    crc = (uint8)(crc ^ dataByte);
    for (uint8 bit = 0U; bit < 8U; bit++) {
        if ((crc & 0x80U) != 0U) {
            crc = (uint8)((uint8)(crc << 1U) ^ COMPROTECTION_CRC_POLY);
        } else {
            crc = (uint8)(crc << 1U);
        }
    }
    return crc;
}

uint8 ComProtection_ComputeCrc8(uint16 protectionDataId,
                                const uint8 payload[COMPROTECTION_PAYLOAD_LENGTH],
                                uint8 crcByteIndex) {
    uint8 working[COMPROTECTION_PAYLOAD_LENGTH];
    (void)memcpy(working, payload, sizeof(working));
    if (crcByteIndex < COMPROTECTION_PAYLOAD_LENGTH) {
        working[crcByteIndex] = 0U;
    }

    uint8 crc = COMPROTECTION_CRC_INIT;
    crc = Crc8UpdateByte(crc, (uint8)(protectionDataId >> 8U));
    crc = Crc8UpdateByte(crc, (uint8)(protectionDataId & 0xFFU));
    for (uint8 i = 0U; i < COMPROTECTION_PAYLOAD_LENGTH; i++) {
        crc = Crc8UpdateByte(crc, working[i]);
    }

    return (uint8)(crc ^ COMPROTECTION_CRC_XOROUT);
}

boolean ComProtection_IsCounterValid(boolean hasLast, uint8 lastCounter, uint8 receivedCounter) {
    if (!hasLast) {
        return TRUE;
    }
    uint8 expected = (uint8)((lastCounter + 1U) & 0x0FU);
    return (receivedCounter == expected) ? TRUE : FALSE;
}
