/**
 * @file Dem.h
 * @brief Diagnostic Event Manager (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 10.2. Tracks the current
 * status (passed/failed), occurrence counter, and last-failed/last-passed
 * monotonic timestamps for a fixed, project-local set of diagnostic
 * events. All timestamps come from `Gpt` (section 8.1: every timeout in
 * the project derives time from `Gpt`), never from a direct clock read.
 *
 * Simplification, documented here per section 20's rule: `occurrenceCount`
 * increments only on a passed -> failed transition, not on every repeated
 * FAILED report while already failed (a repeated FAILED report still
 * refreshes `lastFailedTimestampMs`). This models "how many times the
 * fault appeared" rather than "how many times it was polled as still
 * present", which is the more useful number for the log lines in section
 * 13.4 and for IT-02/IT-06's pass/fail assertions.
 */
#ifndef DEM_H
#define DEM_H

#include "Gpt.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint16 Dem_EventIdType;
typedef uint8 Dem_EventStatusType;

#define DEM_EVENT_STATUS_PASSED ((Dem_EventStatusType)0U)
#define DEM_EVENT_STATUS_FAILED ((Dem_EventStatusType)1U)

/* Project-local diagnostic event registry (spec section 10.2's table).
 * Event ID 0 is reserved/invalid on purpose, so a zero-initialized
 * Dem_EventIdType is never mistaken for a real event. Most of these are
 * not yet set by any caller until later phases add the detection logic
 * (COM timeout supervision in Phase 4, IoHwAb plausibility in Phase 5,
 * Display link supervision in Phase 6) — they are declared now so the
 * registry has one single home. */
#define DEM_EVENT_VEHICLE_STATUS_TIMEOUT ((Dem_EventIdType)1U)
#define DEM_EVENT_VEHICLE_STATUS_INTEGRITY ((Dem_EventIdType)2U)
#define DEM_EVENT_CLUSTER_STATUS_TIMEOUT ((Dem_EventIdType)3U)
#define DEM_EVENT_CLUSTER_STATUS_INTEGRITY ((Dem_EventIdType)4U)
#define DEM_EVENT_SENSOR_LINK_LOST ((Dem_EventIdType)5U)
#define DEM_EVENT_SPEED_SENSOR_RANGE ((Dem_EventIdType)6U)
#define DEM_EVENT_DISPLAY_LINK_LOST ((Dem_EventIdType)7U)

#define DEM_EVENT_ID_MIN ((Dem_EventIdType)1U)
#define DEM_EVENT_ID_MAX ((Dem_EventIdType)7U)

typedef struct {
    Dem_EventStatusType currentStatus;
    uint32 occurrenceCount;
    Gpt_ValueType lastFailedTimestampMs;
    Gpt_ValueType lastPassedTimestampMs;
    boolean everReported;
} Dem_EventRecordType;

void Dem_Init(void);
void Dem_DeInit(void);

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType event_id, Dem_EventStatusType status);

/** Phase 1: reserved for future periodic aggregation (e.g. debounce);
 *  currently a documented no-op, present because section 7.4 requires the
 *  API to exist. */
void Dem_MainFunction(void);

Std_ReturnType Dem_GetEventRecord(Dem_EventIdType event_id, Dem_EventRecordType *out);

#endif /* DEM_H */
