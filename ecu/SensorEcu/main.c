/**
 * @file main.c
 * @brief SensorEcu process entry point.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 8.1, 9 and 15.4.
 *
 * This file owns exactly three things a module may not: the command line,
 * the signal handlers, and the outer real-time loop. Everything else is
 * EcuM's (section 9.2). Note that it reads time through `Gpt`'s driver API
 * for the optional `--duration` stop; it never calls the host clock
 * directly, so `REQ-ARCH-009` is unaffected.
 */

/* sigaction and nanosleep are POSIX.1; the project builds -std=c11 with
 * extensions disabled, so they must be requested explicitly. */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "EcuM.h"
#include "Gpt.h"
#include "Logger.h"
#include "SensorEcu_Cfg.h"

#define EXIT_USAGE 2
#define EXIT_STARTUP_FAILED 3

#define DEFAULT_CAN_INTERFACE "vcan0"
#define DEFAULT_SENSOR_LINK "/tmp/vhw_sensor.sock"

/* REQ-ECUM-003: the handler only sets this atomic flag. All shutdown work
 * happens in normal process context, in the loop below. */
static volatile sig_atomic_t g_stopRequested;

/* Vai trò: signal handler cho SIGINT/SIGTERM — CHỈ đặt 1 cờ
 * sig_atomic_t, không làm gì khác (an toàn để gọi từ ngữ cảnh signal,
 * REQ-ECUM-003). Việc tắt máy thật sự xảy ra ở vòng lặp main() bên dưới. */
static void HandleStopSignal(int signalNumber) {
    (void)signalNumber;
    g_stopRequested = 1;
}

/* Vai trò: đăng ký HandleStopSignal cho cả SIGINT (Ctrl+C) và SIGTERM
 * (kill mặc định) — Ctrl+C và dừng có kiểm soát đều đi cùng 1 đường. */
static void InstallSignalHandlers(void) {
    struct sigaction action;
    (void)memset(&action, 0, sizeof(action));
    action.sa_handler = HandleStopSignal;
    (void)sigemptyset(&action.sa_mask);
    (void)sigaction(SIGINT, &action, NULL);
    (void)sigaction(SIGTERM, &action, NULL);
}

/* Vai trò: in hướng dẫn dòng lệnh ra stdout. */
static void PrintUsage(const char *program) {
    (void)printf("Usage: %s [options]\n"
                 "  --can-if <name>        SocketCAN interface (default %s)\n"
                 "  --sensor-link <path>   virtual sensor line (default %s)\n"
                 "  --duration <seconds>   stop automatically; 0 means run until a signal\n"
                 "  --log-level <error|warn|info|debug>\n"
                 "  --help\n",
                 program, DEFAULT_CAN_INTERFACE, DEFAULT_SENSOR_LINK);
}

/** Vai trò: parse tham số dòng lệnh (--can-if, --sensor-link, --duration,
 * --log-level, --help) thành EcuM_PostBuildType — đây là "post-build
 * configuration" mà EcuM_Init sẽ dùng.
 * Returns 0 on success, EXIT_USAGE on a bad argument, -1 when --help. */
static int ParseArguments(int argc, char **argv, EcuM_PostBuildType *out) {
    out->canInterface = DEFAULT_CAN_INTERFACE;
    out->hwLinkPath = DEFAULT_SENSOR_LINK;
    out->hwLinkEnabled = TRUE;
    out->logLevel = LOGGER_LEVEL_INFO;
    out->durationSeconds = 0U;

    for (int i = 1; i < argc; i++) {
        boolean hasValue = ((i + 1) < argc) ? TRUE : FALSE;
        if (strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return -1;
        } else if ((strcmp(argv[i], "--can-if") == 0) && hasValue) {
            out->canInterface = argv[++i];
        } else if ((strcmp(argv[i], "--sensor-link") == 0) && hasValue) {
            out->hwLinkPath = argv[++i];
        } else if ((strcmp(argv[i], "--duration") == 0) && hasValue) {
            out->durationSeconds = (uint32)strtoul(argv[++i], NULL, 10);
        } else if ((strcmp(argv[i], "--log-level") == 0) && hasValue) {
            if (Logger_ParseLevel(argv[++i], &out->logLevel) != E_OK) {
                (void)fprintf(stderr, "unknown log level: %s\n", argv[i]);
                return EXIT_USAGE;
            }
        } else {
            (void)fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]);
            PrintUsage(argv[0]);
            return EXIT_USAGE;
        }
    }
    return 0;
}

/* Vai trò: ĐIỂM VÀO của cả tiến trình SensorEcu — parse tham số, đăng ký
 * signal handler, build config, gọi EcuM_Init (nếu fail thì thoát với mã
 * lỗi — CAN không mở được là fatal, REQ-ECUM-001), rồi chạy VÒNG LẶP
 * CHÍNH: gọi EcuM_MainFunction() liên tục, ngủ 250µs giữa các lần (ngắn
 * hơn hẳn chu kỳ task 1ms ngắn nhất — vì nanosleep() luôn ngủ lố hơn yêu
 * cầu, ngủ đúng 1ms sẽ làm task 1ms trễ hạn liên tục), dừng khi nhận tín
 * hiệu hoặc hết --duration, rồi gọi EcuM_Shutdown() để tắt sạch sẽ. */
int main(int argc, char **argv) {
    EcuM_PostBuildType postBuild;
    int parseResult = ParseArguments(argc, argv, &postBuild);
    if (parseResult == -1) {
        return 0;
    }
    if (parseResult != 0) {
        return parseResult;
    }

    InstallSignalHandlers();

    const EcuM_ConfigType *config = SensorEcu_BuildConfig(&postBuild);
    if (config == NULL) {
        (void)fprintf(stderr, "SensorEcu: configuration could not be assembled\n");
        return EXIT_STARTUP_FAILED;
    }

    /* REQ-ECUM-001: a CAN interface that cannot be opened is fatal. */
    if (EcuM_Init(config) != E_OK) {
        (void)fprintf(stderr, "SensorEcu: startup failed\n");
        EcuM_Shutdown();
        return EXIT_STARTUP_FAILED;
    }

    /* Section 8.1: the outer loop calls one scheduling pass and may sleep
     * between passes. The sleep is a QUARTER of the shortest task period,
     * not the period itself: nanosleep always overshoots its request, so
     * sleeping a full millisecond makes the 1 ms task miss a release on
     * most iterations and fills the Det log with scheduler overruns. A
     * shorter poll costs CPU and buys deterministic release detection. */
    const struct timespec tick = {.tv_sec = 0, .tv_nsec = 250000L};
    const Gpt_ValueType durationMs = postBuild.durationSeconds * 1000U;

    while (g_stopRequested == 0) {
        EcuM_MainFunction();

        if ((durationMs != 0U) && (Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE) >= durationMs)) {
            Logger_Log(LOGGER_LEVEL_INFO, "SensorEcu", "--duration reached, stopping");
            break;
        }
        (void)nanosleep(&tick, NULL);
    }

    if (g_stopRequested != 0) {
        Logger_Log(LOGGER_LEVEL_INFO, "SensorEcu", "stop signal received");
    }
    EcuM_RequestShutdown();
    EcuM_Shutdown();
    return 0;
}
