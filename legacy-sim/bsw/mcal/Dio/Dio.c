#include "Dio.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Det.h"
#include "Det_ModuleIds.h"
#include "VirtualHw.h"

#define DIO_API_ID_INIT ((Det_ApiIdType)1U)
#define DIO_API_ID_READ_CHANNEL ((Det_ApiIdType)2U)
#define DIO_API_ID_GET_AGE ((Det_ApiIdType)3U)
#define DIO_API_ID_MAIN_FUNCTION_READ ((Det_ApiIdType)4U)

#define DIO_MAX_CHANNELS ((uint8)4U)

typedef struct {
    boolean configured;
    Dio_ChannelConfigType config;
} Dio_ChannelStateType;

static Dio_ChannelStateType g_channels[DIO_MAX_CHANNELS];
static boolean g_initialized;

/* Vai trò: dựng chuỗi khoá "DIO<n>" để tra trong payload text từ
 * VirtualHw. */
static void BuildDioKey(uint8 channelId, char *outKey, size_t outKeySize) {
    (void)snprintf(outKey, outKeySize, "DIO%u", (unsigned)channelId);
}

/* Vai trò: kiểm tra channel id hợp lệ + đã init + đã configured. */
static boolean ChannelIsValid(Dio_ChannelType channel_id) {
    return g_initialized && (channel_id < DIO_MAX_CHANNELS) && g_channels[channel_id].configured;
}

/* Vai trò: nạp config các channel DIO (mỗi channel gắn 1 virtual hw link
 * + mức mặc định khi chưa có dữ liệu), validate rồi bật cờ đã init. */
void Dio_Init(const Dio_ConfigType *config) {
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, 0U, DIO_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if ((config->channels == NULL) || (config->numChannels == 0U) ||
        (config->numChannels > DIO_MAX_CHANNELS)) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, 0U, DIO_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    (void)memset(g_channels, 0, sizeof(g_channels));
    for (uint8 i = 0U; i < config->numChannels; i++) {
        Dio_ChannelType ch = config->channels[i].channelId;
        if (ch >= DIO_MAX_CHANNELS) {
            (void)Det_ReportError(DET_MODULE_ID_DIO, i, DIO_API_ID_INIT, DET_ERR_INVALID_CONFIG);
            continue;
        }
        g_channels[ch].configured = TRUE;
        g_channels[ch].config = config->channels[i];
    }
    g_initialized = TRUE;
}

/* Vai trò: tắt module (chỉ hạ cờ init). */
void Dio_DeInit(void) {
    g_initialized = FALSE;
}

/* Vai trò: đọc mức tín hiệu số (HIGH/LOW) mới nhất — lấy payload từ
 * VirtualHw, parse key "DIOn"; nếu chưa từng có dữ liệu hoặc parse lỗi
 * thì trả về mức mặc định đã cấu hình (section 11.2). */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType channel_id) {
    if (!ChannelIsValid(channel_id)) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, channel_id, DIO_API_ID_READ_CHANNEL,
                              DET_ERR_INVALID_PARAM);
        return STD_LOW;
    }

    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType ageMs;
    boolean isFresh;
    if (!VirtualHw_GetLatestPayload(g_channels[channel_id].config.virtualHwLinkId, payload,
                                    sizeof(payload), &ageMs, &isFresh)) {
        return g_channels[channel_id]
            .config.defaultLevel; /* section 11.2: default before the first datagram */
    }

    char key[8];
    BuildDioKey(g_channels[channel_id].config.channelId, key, sizeof(key));
    uint32 value;
    if (VirtualHw_ParseUint(payload, key, &value) != E_OK) {
        return g_channels[channel_id].config.defaultLevel;
    }

    return (value != 0U) ? STD_HIGH : STD_LOW;
}

/* Vai trò: trả về tuổi (ms) của mẫu dữ liệu mới nhất trên channel này. */
Gpt_ValueType Dio_GetChannelAge(Dio_ChannelType channel_id) {
    if (!ChannelIsValid(channel_id)) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, channel_id, DIO_API_ID_GET_AGE,
                              DET_ERR_INVALID_PARAM);
        return UINT32_MAX;
    }

    char payload[VIRTUALHW_MAX_PAYLOAD + 1U];
    Gpt_ValueType ageMs;
    boolean isFresh;
    if (!VirtualHw_GetLatestPayload(g_channels[channel_id].config.virtualHwLinkId, payload,
                                    sizeof(payload), &ageMs, &isFresh)) {
        return UINT32_MAX;
    }
    return ageMs;
}

/* Vai trò: hàm chạy theo chu kỳ, kéo VirtualHw đọc datagram mới nhất cho
 * từng channel đã cấu hình. */
void Dio_MainFunction_Read(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_DIO, 0U, DIO_API_ID_MAIN_FUNCTION_READ, DET_ERR_UNINIT);
        return;
    }
    for (uint8 c = 0U; c < DIO_MAX_CHANNELS; c++) {
        if (g_channels[c].configured) {
            VirtualHw_MainFunction_Read(g_channels[c].config.virtualHwLinkId);
        }
    }
}
