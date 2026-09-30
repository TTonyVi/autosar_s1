/**
 * @file main.c
 * @brief ClusterEcu process entry point.
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

#include "ClusterEcu_Cfg.h"
#include "EcuM.h"
#include "Gpt.h"
#include "Logger.h"

#define EXIT_USAGE 2
#define EXIT_STARTUP_FAILED 3

#define DEFAULT_CAN_INTERFACE "vcan0"
#define DEFAULT_DISPLAY_LINK "/tmp/vhw_cluster_display.sock"

/* Vai trò: signal handler cho SIGINT/SIGTERM — CHỈ đặt 1 cờ
 * boolean trong EcuM, không làm gì khác (an toàn để gọi từ ngữ cảnh signal,
 * REQ-ECUM-003). Việc tắt máy thật sự xảy ra trong ShutdownOS của OS. */
static void HandleStopSignal(int signalNumber) {
    (void)signalNumber;
    EcuM_RequestShutdown();
}

/* Vai trò: đăng ký handler cho SIGINT/SIGTERM. */
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
                 "  --display-link <path>   virtual display line, empty string disables the panel "
                 "(default %s)\n"
                 "  --duration <seconds>   stop automatically; 0 means run until a signal\n"
                 "  --log-level <error|warn|info|debug>\n"
                 "  --help\n",
                 program, DEFAULT_CAN_INTERFACE, DEFAULT_DISPLAY_LINK);
}

/** Vai trò: parse tham số dòng lệnh thành EcuM_PostBuildType. Khác
 * SensorEcu ở chỗ `--display-link` rỗng ("") thì TẮT LUÔN driver display
 * (chỉ còn log, section 15.4) thay vì coi là lỗi.
 * Returns 0 on success, EXIT_USAGE on a bad argument, -1 when --help. */
static int ParseArguments(int argc, char **argv, EcuM_PostBuildType *out) {
    out->canInterface = DEFAULT_CAN_INTERFACE;
    out->hwLinkPath = DEFAULT_DISPLAY_LINK;
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
        } else if ((strcmp(argv[i], "--display-link") == 0) && hasValue) {
            out->hwLinkPath = argv[++i];
            /* Section 15.4: an empty string disables the display driver and
             * the ECU logs only. */
            out->hwLinkEnabled = (out->hwLinkPath[0] != '\0') ? TRUE : FALSE;
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

/* Vai trò: ĐIỂM VÀO của tiến trình ClusterEcu — giống hệt cấu trúc
 * SensorEcu/main.c: parse tham số → đăng ký signal handler → build config
 * → EcuM_Init (CAN mở lỗi là fatal) → vòng lặp chính gọi
 * EcuM_MainFunction() + ngủ 250µs mỗi vòng → dừng khi có tín hiệu/hết
 * --duration → EcuM_Shutdown(). */
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

    const EcuM_ConfigType *config = ClusterEcu_BuildConfig(&postBuild);
    if (config == NULL) {
        (void)fprintf(stderr, "ClusterEcu: configuration could not be assembled\n");
        return EXIT_STARTUP_FAILED;
    }

    /* REQ-ECUM-001: a CAN interface that cannot be opened is fatal.
     * EcuM_Init runs Phase 1 Pre-OS and calls StartOS(OSDEFAULTAPPMODE).
     * On success, StartOS never returns! */
    if (EcuM_Init(config) != E_OK) {
        (void)fprintf(stderr, "ClusterEcu: startup failed\n");
        EcuM_Shutdown();
        return EXIT_STARTUP_FAILED;
    }

    return 0;
}
