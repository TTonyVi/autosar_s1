#include "VirtualHw_Fake.h"

#include <stddef.h>
#include <string.h>

#define VIRTUALHW_FAKE_MAX_LINKS ((uint8)4U)

typedef struct {
    boolean configured;
    VirtualHw_LinkConfigType config;

    boolean hasLatest;
    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    uint16 payloadLength;
    Gpt_ValueType receiveTimeMs;

    Std_ReturnType sendResult;
    uint32 sendCount;
} VirtualHwFake_LinkStateType;

static VirtualHwFake_LinkStateType g_links[VIRTUALHW_FAKE_MAX_LINKS];

void VirtualHw_Init(const VirtualHw_ConfigType *config) {
    (void)memset(g_links, 0, sizeof(g_links));
    if ((config != NULL) && (config->links != NULL)) {
        for (uint8 i = 0U; i < config->numLinks; i++) {
            VirtualHw_LinkIdType id = config->links[i].linkId;
            if (id < VIRTUALHW_FAKE_MAX_LINKS) {
                g_links[id].configured = TRUE;
                g_links[id].config = config->links[i];
                g_links[id].sendResult = E_OK;
            }
        }
    }
}

void VirtualHw_DeInit(void) {
    (void)memset(g_links, 0, sizeof(g_links));
}

void VirtualHw_MainFunction_Read(VirtualHw_LinkIdType linkId) {
    (void)linkId; /* the fake has no real socket to drain; tests deliver
                      payloads directly via VirtualHwFake_Deliver */
}

boolean VirtualHw_GetLatestPayload(VirtualHw_LinkIdType linkId, char *buffer, uint16 bufferSize,
                                   Gpt_ValueType *ageMsOut, boolean *isFreshOut) {
    if ((linkId >= VIRTUALHW_FAKE_MAX_LINKS) || (!g_links[linkId].hasLatest)) {
        return FALSE;
    }

    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    Gpt_ValueType age = now - g_links[linkId].receiveTimeMs;

    if ((buffer != NULL) && (bufferSize > 0U)) {
        uint16 copyLen = g_links[linkId].payloadLength;
        if (copyLen >= bufferSize) {
            copyLen = (uint16)(bufferSize - 1U);
        }
        (void)memcpy(buffer, g_links[linkId].payload, copyLen);
        buffer[copyLen] = '\0';
    }
    if (ageMsOut != NULL) {
        *ageMsOut = age;
    }
    if (isFreshOut != NULL) {
        *isFreshOut = (age <= g_links[linkId].config.stalenessTimeoutMs) ? TRUE : FALSE;
    }
    return TRUE;
}

Std_ReturnType VirtualHw_Send(VirtualHw_LinkIdType linkId, const char *payload,
                              uint16 payloadLength) {
    (void)payload;
    (void)payloadLength;
    if (linkId >= VIRTUALHW_FAKE_MAX_LINKS) {
        return E_NOT_OK;
    }
    g_links[linkId].sendCount++;
    return g_links[linkId].sendResult;
}

void VirtualHwFake_Reset(void) {
    (void)memset(g_links, 0, sizeof(g_links));
}

void VirtualHwFake_Deliver(VirtualHw_LinkIdType linkId, const char *payload) {
    if (linkId >= VIRTUALHW_FAKE_MAX_LINKS) {
        return;
    }
    size_t len = strlen(payload);
    if (len > VIRTUALHW_MAX_PAYLOAD) {
        len = VIRTUALHW_MAX_PAYLOAD;
    }
    (void)memcpy(g_links[linkId].payload, payload, len);
    g_links[linkId].payload[len] = '\0';
    g_links[linkId].payloadLength = (uint16)len;
    g_links[linkId].receiveTimeMs = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    g_links[linkId].hasLatest = TRUE;
}

void VirtualHwFake_ClearLatest(VirtualHw_LinkIdType linkId) {
    if (linkId < VIRTUALHW_FAKE_MAX_LINKS) {
        g_links[linkId].hasLatest = FALSE;
    }
}

void VirtualHwFake_SetSendResult(VirtualHw_LinkIdType linkId, Std_ReturnType result) {
    if (linkId < VIRTUALHW_FAKE_MAX_LINKS) {
        g_links[linkId].sendResult = result;
    }
}

uint32 VirtualHwFake_GetSendCount(VirtualHw_LinkIdType linkId) {
    return (linkId < VIRTUALHW_FAKE_MAX_LINKS) ? g_links[linkId].sendCount : 0U;
}
