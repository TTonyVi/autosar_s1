#include "Logger.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Gpt.h"

#define LOGGER_MAX_MESSAGE 192U

static const char *g_ecuName = "Ecu";
static Logger_LevelType g_level = LOGGER_LEVEL_INFO;
static boolean g_initialized = FALSE;
static uint32 g_emittedCount;
static boolean g_timeBaseReady;

static const char *const kSeverityNames[4] = {"ERROR", "WARN", "INFO", "DEBUG"};

/* Vai trò: nạp tên ECU (hiện trong mỗi dòng log) + mức log tối thiểu, bật
 * cờ đã init. */
void Logger_Init(const char *ecuName, Logger_LevelType level) {
    g_ecuName = (ecuName != NULL) ? ecuName : "Ecu";
    g_level = (level <= LOGGER_LEVEL_DEBUG) ? level : LOGGER_LEVEL_INFO;
    g_emittedCount = 0U;
    g_timeBaseReady = FALSE;
    g_initialized = TRUE;
}

/* Vai trò: tắt module — flush nốt output còn kẹt trong buffer trước khi
 * dừng, hạ hết cờ. */
void Logger_DeInit(void) {
    (void)fflush(stdout);
    g_timeBaseReady = FALSE;
    g_initialized = FALSE;
}

/* Vai trò: báo cho Logger biết Gpt đã init xong, từ giờ timestamp trong
 * log là thời gian thật thay vì luôn in 0. */
void Logger_NotifyTimeBaseReady(void) {
    g_timeBaseReady = TRUE;
}

/* Vai trò: đổi mức log tối thiểu lúc chạy (vd chỉ in ERROR/WARN). */
void Logger_SetLevel(Logger_LevelType level) {
    if (level <= LOGGER_LEVEL_DEBUG) {
        g_level = level;
    }
}

/* Vai trò: getter — mức log hiện tại. */
Logger_LevelType Logger_GetLevel(void) {
    return g_level;
}

/* Vai trò: API TRUNG TÂM để in 1 dòng log — lọc theo severity/level, định
 * dạng chuỗi kiểu printf, in ra stdout theo đúng khuôn "[time][ecu][module]
 * [severity] message" (section 13.4), rồi flush ngay lập tức (bugfix
 * thật: nếu không flush, khi stdout bị redirect ra file thì log chỉ hiện
 * ra lúc process thoát, làm hỏng khả năng "xem log ECU đang chạy" —
 * IT-06 đã bắt được lỗi này). */
void Logger_Log(Logger_LevelType severity, const char *module, const char *format, ...) {
    if ((!g_initialized) || (severity > g_level) || (module == NULL) || (format == NULL)) {
        return;
    }

    char message[LOGGER_MAX_MESSAGE];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (written < 0) {
        return;
    }

    /* 0 until Gpt_Init has run; see the header for why this is not a
     * Gpt read. */
    unsigned timestampMs =
        g_timeBaseReady ? (unsigned)Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE) : 0U;

    /* Section 13.4 line shape, fixed field order. */
    (void)printf("[%06u ms] [%s] [%s] %s %s\n", timestampMs, g_ecuName, module,
                 kSeverityNames[severity], message);
    /* stdout is fully buffered (not line-buffered) whenever it is not a
     * TTY -- i.e. whenever output is redirected to a file, piped, or run
     * under a supervisor, which is the normal case for this project's own
     * demo scripts and integration tests. Without this flush, section
     * 15.8's "ECU stdout logs" observation surface silently does nothing
     * until the process exits: a real bug found by IT-06 actually reading
     * the log of a still-running ECU. Section 13.4 already bounds log
     * volume, so a flush per line costs nothing this project cares about. */
    (void)fflush(stdout);
    g_emittedCount++;
}

/* Vai trò: parse chuỗi text ("error"/"warn"/"info"/"debug") — vd từ tham
 * số dòng lệnh --log-level — thành enum Logger_LevelType. */
Std_ReturnType Logger_ParseLevel(const char *text, Logger_LevelType *out) {
    if ((text == NULL) || (out == NULL)) {
        return E_NOT_OK;
    }
    if (strcmp(text, "error") == 0) {
        *out = LOGGER_LEVEL_ERROR;
    } else if (strcmp(text, "warn") == 0) {
        *out = LOGGER_LEVEL_WARN;
    } else if (strcmp(text, "info") == 0) {
        *out = LOGGER_LEVEL_INFO;
    } else if (strcmp(text, "debug") == 0) {
        *out = LOGGER_LEVEL_DEBUG;
    } else {
        return E_NOT_OK;
    }
    return E_OK;
}

/* Vai trò: getter — tổng số dòng log đã in ra thành công. */
uint32 Logger_GetEmittedCount(void) {
    return g_emittedCount;
}
