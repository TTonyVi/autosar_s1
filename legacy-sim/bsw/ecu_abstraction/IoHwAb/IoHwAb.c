#include "IoHwAb.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Adc.h"
#include "Dem.h"
#include "Det.h"
#include "Det_ModuleIds.h"
#include "Dio.h"
#include "Display.h"
#include "Gpt.h"

#define IOHWAB_API_ID_INIT ((Det_ApiIdType)1U)
#define IOHWAB_API_ID_GET_SPEED ((Det_ApiIdType)2U)
#define IOHWAB_API_ID_GET_IGNITION ((Det_ApiIdType)3U)
#define IOHWAB_API_ID_SET_DISPLAY ((Det_ApiIdType)4U)
#define IOHWAB_API_ID_MAIN_FUNCTION ((Det_ApiIdType)5U)

typedef struct {
    uint32 value; /* scaled output value, meaning depends on the signal */
    IoHwAb_ValidityType validity;
} IoHwAb_SampleType;

static const IoHwAb_ConfigType *g_config;
static boolean g_initialized;
static IoHwAb_SampleType g_samples[IOHWAB_NUM_SIGNALS];
static uint32 g_invalidSampleCount;
static uint32 g_validSampleCount;
static uint32 g_displayWriteFailureCount;
static uint32 g_displaySuccessSnapshot;
static Gpt_ValueType g_lastDisplaySuccessMs;
static boolean g_hasDisplaySuccess;
static boolean g_displayLinkReported;

static const char *const kStateNames[4] = {"OFF", "NORMAL", "OVERSPEED", "COMM_FAULT"};

/**
 * Vai trò: quy đổi counts thô (vd ADC counts) sang đơn vị vật lý (vd
 * km/h×100) bằng công thức tuyến tính cấu hình sẵn (nhân/chia + offset),
 * rồi kẹp trong khoảng [outputMin, outputMax] — ĐÂY LÀ NƠI "diễn giải ý
 * nghĩa" dữ liệu thô, việc mà Adc/Dio không được làm (section 4.1.2).
 */
static uint32 ScaleAndClamp(const IoHwAb_SignalConfigType *cfg, uint32 rawCounts) {
    uint32 scaled = (rawCounts * cfg->scalingNumerator) / cfg->scalingDenominator;

    sint32 withOffset = (sint32)scaled + cfg->offset;
    if (withOffset < 0) {
        withOffset = 0;
    }
    uint32 result = (uint32)withOffset;

    if (result < cfg->outputMin) {
        result = cfg->outputMin;
    }
    if (result > cfg->outputMax) {
        result = cfg->outputMax;
    }
    return result;
}

/**
 * Vai trò: đọc 1 signal từ driver nguồn (Adc hoặc Dio), đánh giá "còn
 * tươi không" (fresh) + "hợp lý không" (plausible, trong khoảng cấu
 * hình), rồi scale ra giá trị vật lý — chỉ khi CẢ HAI điều kiện đều đạt
 * mới đánh dấu VALID; ngược lại trả về 0 + INVALID (REQ-IOHWAB-001: không
 * bao giờ trả 1 giá trị cũ mà lại gắn nhãn "hợp lệ").
 *
 * Samples one signal's driver and produces value + validity.
 *
 * `freshOut` reports whether THIS signal had a fresh value; the caller
 * aggregates it. DEM_EVENT_SENSOR_LINK_LOST is deliberately NOT reported
 * here: it describes the health of the sensor line as a whole (section
 * 10.2, "no fresh ADC/DIO value for >500 ms"), and both signals arrive in
 * the same datagram. Reporting it per signal let the last signal sampled
 * overwrite the verdict of the first.
 */
static void SampleSignal(const IoHwAb_SignalConfigType *cfg, IoHwAb_SampleType *out,
                         boolean *freshOut) {
    uint32 rawCounts = 0U;
    Gpt_ValueType ageMs = 0U;
    boolean everReceived = FALSE;

    if (cfg->sourceDriver == IOHWAB_SOURCE_ADC) {
        Adc_ValueGroupType counts = 0U;
        Adc_GroupType group = (Adc_GroupType)cfg->sourceChannel;
        everReceived = (Adc_GetGroupStatus(group) != ADC_IDLE) ? TRUE : FALSE;
        if (Adc_ReadGroup(group, &counts) == E_OK) {
            rawCounts = (uint32)counts;
        }
        ageMs = Adc_GetGroupAge(group);
    } else {
        Dio_ChannelType channel = (Dio_ChannelType)cfg->sourceChannel;
        rawCounts = (Dio_ReadChannel(channel) == STD_HIGH) ? 1U : 0U;
        ageMs = Dio_GetChannelAge(channel);
        /* Dio always reports a level (its configured default before the
         * first datagram), so freshness is the only evidence of a real
         * value — age is capped at the timeout by the driver when nothing
         * has ever arrived. */
        everReceived = (ageMs < cfg->inputTimeoutMs) ? TRUE : FALSE;
    }

    /* REQ-IOHWAB-002: stale or never-sampled input is INVALID. */
    boolean fresh = (everReceived && (ageMs <= cfg->inputTimeoutMs)) ? TRUE : FALSE;

    /* Section 4.1.2: counts outside the plausibility window are invalid
     * and reported through DEM (REQ-IOHWAB-004). */
    boolean plausible =
        ((rawCounts >= cfg->plausibilityMinCounts) && (rawCounts <= cfg->plausibilityMaxCounts))
            ? TRUE
            : FALSE;

    if (cfg->sourceDriver == IOHWAB_SOURCE_ADC) {
        /* Range is a property of this one channel, so it stays per signal. */
        (void)Dem_SetEventStatus(DEM_EVENT_SPEED_SENSOR_RANGE,
                                 plausible ? DEM_EVENT_STATUS_PASSED : DEM_EVENT_STATUS_FAILED);
    }
    *freshOut = fresh;

    if (fresh && plausible) {
        out->value = ScaleAndClamp(cfg, rawCounts);
        out->validity = IOHWAB_VALID;
        g_validSampleCount++;
    } else {
        /* REQ-IOHWAB-001: never hand back a stale value marked valid. The
         * value is zeroed so a caller that ignores validity still cannot
         * read a frozen speed. */
        out->value = 0U;
        out->validity = IOHWAB_INVALID;
        g_invalidSampleCount++;
    }
}

/* Vai trò: nạp bảng cấu hình signal (mỗi signal biết nguồn Adc/Dio, công
 * thức scale, khoảng plausibility, timeout), validate all-or-nothing. */
void IoHwAb_Init(const IoHwAb_ConfigType *config) {
    g_initialized = FALSE;
    g_config = NULL;
    (void)memset(g_samples, 0, sizeof(g_samples));
    for (uint8 i = 0U; i < (uint8)IOHWAB_NUM_SIGNALS; i++) {
        g_samples[i].validity = IOHWAB_INVALID;
    }
    g_invalidSampleCount = 0U;
    g_validSampleCount = 0U;
    g_displayWriteFailureCount = 0U;
    g_displaySuccessSnapshot = 0U;
    g_lastDisplaySuccessMs = 0U;
    g_hasDisplaySuccess = FALSE;
    g_displayLinkReported = FALSE;

    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if (config->numSignals > (uint8)IOHWAB_NUM_SIGNALS) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }
    if ((config->numSignals > 0U) && (config->signals == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if (config->hasDisplay && (config->displayTimeoutMs == 0U)) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    /* All-or-nothing validation (section 11.3). */
    for (uint8 i = 0U; i < config->numSignals; i++) {
        const IoHwAb_SignalConfigType *sig = &config->signals[i];
        if ((sig->signalId != (IoHwAb_SignalIdType)i) || (sig->scalingDenominator == 0U) ||
            (sig->outputMax < sig->outputMin) ||
            (sig->plausibilityMaxCounts < sig->plausibilityMinCounts) ||
            (sig->inputTimeoutMs == 0U) ||
            ((sig->sourceDriver != IOHWAB_SOURCE_ADC) &&
             (sig->sourceDriver != IOHWAB_SOURCE_DIO))) {
            (void)Det_ReportError(DET_MODULE_ID_IOHWAB, i, IOHWAB_API_ID_INIT,
                                  DET_ERR_INVALID_CONFIG);
            return;
        }
    }

    g_config = config;
    g_initialized = TRUE;
}

/* Vai trò: tắt module (hạ cờ init, bỏ tham chiếu config). */
void IoHwAb_DeInit(void) {
    g_initialized = FALSE;
    g_config = NULL;
}

/* Vai trò: hàm chạy theo chu kỳ — lấy mẫu TẤT CẢ signal đã cấu hình qua
 * SampleSignal, tổng hợp thành verdict "cả sensor line có mất kết nối
 * không" (1 tín hiệu cũ là cả line coi như mất — REQ-IOHWAB-004), và
 * theo dõi display link mất bao lâu (chỉ báo mất sau khi ghi lỗi liên
 * tục quá displayTimeoutMs, không phải 1 lần fail là báo ngay). */
void IoHwAb_MainFunction(void) {
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_MAIN_FUNCTION,
                              DET_ERR_UNINIT);
        return;
    }

    boolean anyStale = FALSE;
    for (uint8 i = 0U; i < g_config->numSignals; i++) {
        boolean fresh = FALSE;
        SampleSignal(&g_config->signals[i], &g_samples[i], &fresh);
        if (!fresh) {
            anyStale = TRUE;
        }
    }
    if (g_config->numSignals > 0U) {
        /* One verdict for the whole sensor line: if any input on it has gone
         * stale, the line is what is lost (REQ-IOHWAB-004). */
        (void)Dem_SetEventStatus(DEM_EVENT_SENSOR_LINK_LOST,
                                 anyStale ? DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED);
    }

    if (g_config->hasDisplay) {
        /* Section 10.2: the link counts as lost only after writes have been
         * failing continuously for displayTimeoutMs, not on one failure. */
        Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
        if (!g_hasDisplaySuccess) {
            g_lastDisplaySuccessMs = (g_lastDisplaySuccessMs == 0U) ? now : g_lastDisplaySuccessMs;
        }
        boolean lost = ((now - g_lastDisplaySuccessMs) > g_config->displayTimeoutMs) ? TRUE : FALSE;
        if (lost != g_displayLinkReported) {
            (void)Dem_SetEventStatus(DEM_EVENT_DISPLAY_LINK_LOST,
                                     lost ? DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED);
            g_displayLinkReported = lost;
        }
    }
}

/* Vai trò: API cho RTE/SW-C đọc tốc độ xe đã scale — trả cả giá trị lẫn
 * cờ validity, không tự ý quyết định thay SW-C nên làm gì khi invalid. */
Std_ReturnType IoHwAb_GetVehicleSpeedRaw(VehicleSpeedRawType *speed_raw,
                                         IoHwAb_ValidityType *validity) {
    if ((speed_raw == NULL) || (validity == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_GET_SPEED,
                              DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_GET_SPEED, DET_ERR_UNINIT);
        *speed_raw = 0U;
        *validity = IOHWAB_INVALID;
        return E_NOT_OK;
    }

    const IoHwAb_SampleType *sample = &g_samples[IOHWAB_SIGNAL_VEHICLE_SPEED];
    *speed_raw = (VehicleSpeedRawType)sample->value;
    *validity = sample->validity;
    return E_OK;
}

/* Vai trò: API cho RTE/SW-C đọc trạng thái ignition — cùng mẫu với
 * IoHwAb_GetVehicleSpeedRaw. */
Std_ReturnType IoHwAb_GetIgnitionState(IgnitionOnType *ignition_on, IoHwAb_ValidityType *validity) {
    if ((ignition_on == NULL) || (validity == NULL)) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_GET_IGNITION,
                              DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_GET_IGNITION, DET_ERR_UNINIT);
        *ignition_on = FALSE;
        *validity = IOHWAB_INVALID;
        return E_NOT_OK;
    }

    const IoHwAb_SampleType *sample = &g_samples[IOHWAB_SIGNAL_IGNITION];
    *ignition_on = (sample->value != 0U) ? TRUE : FALSE;
    *validity = sample->validity;
    return E_OK;
}

/* Vai trò: API cho RTE/SW-C ghi trạng thái cluster ra màn hình — validate
 * + tự clamp lại speed (không tin tưởng tuyệt đối SW-C — REQ-ASW-006),
 * định dạng thành chuỗi text "STATE=...;SPEED=...;FLAGS=...;T=..." đúng
 * khuôn section 15.5.2, rồi gọi Display_Write. Theo dõi luôn lần ghi
 * thành công gần nhất để IoHwAb_MainFunction tính display link có mất
 * hay không. */
Std_ReturnType IoHwAb_SetClusterDisplay(const IoHwAb_ClusterDisplayType *value) {
    if (value == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_SET_DISPLAY,
                              DET_ERR_NULL_POINTER);
        return E_NOT_OK;
    }
    if (!g_initialized) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_SET_DISPLAY, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    if (!g_config->hasDisplay) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_SET_DISPLAY,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if (value->state > CLUSTER_STATE_COMM_FAULT) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_SET_DISPLAY,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    /* REQ-ASW-006 is the SW-C's rule, but the abstraction clamps too: the
     * panel must never be handed a value it cannot mean. */
    DisplayedSpeedRawType speed = value->displayedSpeedRaw;
    if (speed > VEHICLE_SPEED_RAW_MAX) {
        speed = VEHICLE_SPEED_RAW_MAX;
    }

    /* Section 15.5.2 display line format; field order is fixed. */
    Display_FrameType frame;
    (void)memset(&frame, 0, sizeof(frame));
    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);
    int written =
        snprintf((char *)frame.data, (size_t)IOHWAB_DISPLAY_FRAME_MAX,
                 "STATE=%s;SPEED=%u;FLAGS=0x%02X;T=%u", kStateNames[value->state], (unsigned)speed,
                 (unsigned)(value->flags & CLUSTER_FLAGS_VALID_MASK), (unsigned)now);
    if ((written <= 0) || (written >= (int)IOHWAB_DISPLAY_FRAME_MAX)) {
        (void)Det_ReportError(DET_MODULE_ID_IOHWAB, 0U, IOHWAB_API_ID_SET_DISPLAY,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    frame.length = (uint16)written;

    /* Display_Write returns E_NOT_OK both for a throttled write and for a
     * failed one; the success counter is what distinguishes them without
     * changing the driver's API. */
    uint32 successBefore = Display_GetWriteSuccessCount();
    Std_ReturnType result = Display_Write(&frame);
    if (Display_GetWriteSuccessCount() > successBefore) {
        g_lastDisplaySuccessMs = now;
        g_hasDisplaySuccess = TRUE;
    } else if (result != E_OK) {
        g_displayWriteFailureCount++;
    } else {
        /* unreachable: E_OK always increments the success counter */
    }
    g_displaySuccessSnapshot = Display_GetWriteSuccessCount();

    /* Never fatal (REQ-VHW-005): the caller is told, application behavior
     * is not changed by the answer. */
    return result;
}

/* Vai trò: getter — tổng số lần lấy mẫu bị đánh giá INVALID (cũ hoặc
 * ngoài khoảng plausibility). */
uint32 IoHwAb_GetInvalidSampleCount(void) {
    return g_invalidSampleCount;
}

/* Vai trò: getter — tổng số lần lấy mẫu VALID. */
uint32 IoHwAb_GetValidSampleCount(void) {
    return g_validSampleCount;
}

/* Vai trò: getter — số lần ghi màn hình thất bại (không tính lần bị
 * throttle). */
uint32 IoHwAb_GetDisplayWriteFailureCount(void) {
    return g_displayWriteFailureCount;
}
