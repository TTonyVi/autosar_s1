/**
 * @file Logger.h
 * @brief The logging backend named by spec sections 3.1, 9.2 and 13.4.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md section 13.4 fixes the line shape:
 * monotonic milliseconds, ECU name, module, severity, message —
 *
 *     [001240 ms] [ClusterEcu] [Dem] WARN VEHICLE_STATUS_TIMEOUT failed count=1
 *
 * It is the first thing EcuM initializes (section 9.2 step 1) so that every
 * later initialization failure is reportable rather than silently lost.
 *
 * Timestamps come from `Gpt` — the logger is one of the general-purpose
 * timer's legitimate users (section 8.1); it never reads the OS counter.
 *
 * The logger comes up at init step 1 but `Gpt_Init` only at step 6, so
 * until EcuM calls `Logger_NotifyTimeBaseReady` the logger must NOT read
 * the timer: doing so would make an uninitialized-Gpt Det report out of
 * every early line, at a point where Det itself is not initialized either.
 * Early lines therefore carry timestamp 0, which is the honest answer —
 * the ECU time base does not exist yet.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint8 Logger_LevelType;

#define LOGGER_LEVEL_ERROR ((Logger_LevelType)0U)
#define LOGGER_LEVEL_WARN ((Logger_LevelType)1U)
#define LOGGER_LEVEL_INFO ((Logger_LevelType)2U)
#define LOGGER_LEVEL_DEBUG ((Logger_LevelType)3U)

/** Names the ECU that prefixes every line and sets the threshold. */
void Logger_Init(const char *ecuName, Logger_LevelType level);
void Logger_DeInit(void);

/** Tells the logger the general-purpose time base is running; called by
 *  EcuM immediately after `Gpt_Init` (section 9.2 step 6). */
void Logger_NotifyTimeBaseReady(void);

void Logger_SetLevel(Logger_LevelType level);
Logger_LevelType Logger_GetLevel(void);

/**
 * Emits one line if `severity` is at or above the configured level.
 * Section 13.4: periodic log volume must be bounded and the 1 ms task
 * SHALL never log normal activity — that is a caller obligation; this
 * module only enforces the level threshold and the line format.
 */
void Logger_Log(Logger_LevelType severity, const char *module, const char *format, ...);

/** Parses an `--log-level` argument (section 15.4). */
Std_ReturnType Logger_ParseLevel(const char *text, Logger_LevelType *out);

/** Lines actually emitted since init; used by tests and the status summary. */
uint32 Logger_GetEmittedCount(void);

#endif /* LOGGER_H */
