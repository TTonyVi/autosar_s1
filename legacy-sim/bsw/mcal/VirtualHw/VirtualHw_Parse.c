/**
 * @file VirtualHw_Parse.c
 * @brief Pure "KEY=value" ASCII parsing for the virtual hardware wire
 * format (section 15.5.2). No sockets, no state — kept separate from
 * VirtualHw.c specifically so tests can link this real implementation
 * alongside a fake VirtualHw (tests/fixtures/VirtualHw_Fake.c) instead of
 * duplicating this logic.
 */
#include "VirtualHw.h"

#include <stdlib.h>
#include <string.h>

/* Vai trò: parse text dạng "KEY1=v1;KEY2=v2;..." tìm đúng `key`, đọc giá
 * trị số nguyên không dấu đứng sau dấu "=". Không có state, không đụng
 * socket — logic thuần chuỗi, tách riêng khỏi VirtualHw.c để test có thể
 * link bản thật này chung với 1 VirtualHw giả (fake). */
Std_ReturnType VirtualHw_ParseUint(const char *payload, const char *key, uint32 *out) {
    if ((payload == NULL) || (key == NULL) || (out == NULL)) {
        return E_NOT_OK;
    }

    size_t keyLen = strlen(key);
    const char *segment = payload;

    while (*segment != '\0') {
        if ((strncmp(segment, key, keyLen) == 0) && (segment[keyLen] == '=')) {
            const char *valueStart = &segment[keyLen + 1U];
            char *endPtr = NULL;
            unsigned long value = strtoul(valueStart, &endPtr, 10);
            if (endPtr == valueStart) {
                return E_NOT_OK; /* "KEY=" with no digits following */
            }
            *out = (uint32)value;
            return E_OK;
        }

        const char *nextSegment = strchr(segment, ';');
        if (nextSegment == NULL) {
            break;
        }
        segment = &nextSegment[1];
    }

    return E_NOT_OK;
}
