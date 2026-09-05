#include "Dem.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define DEM_API_ID_SET_EVENT_STATUS ((Det_ApiIdType)1U)

/* Index 0 is reserved/unused so DEM_EVENT_ID_MIN..MAX can be used directly
 * as array indices without an off-by-one translation. */
#define DEM_NUM_EVENT_SLOTS ((uint32)DEM_EVENT_ID_MAX + 1U)

static Dem_EventRecordType g_events[DEM_NUM_EVENT_SLOTS];
static boolean g_initialized;

void Dem_Init(void) {
    (void)memset(g_events, 0, sizeof(g_events));
    g_initialized = TRUE;
}

void Dem_DeInit(void) {
    g_initialized = FALSE;
}

static boolean IsValidEventId(Dem_EventIdType event_id) {
    return (event_id >= DEM_EVENT_ID_MIN) && (event_id <= DEM_EVENT_ID_MAX);
}

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType event_id, Dem_EventStatusType status) {
    if (!g_initialized) {
        return E_NOT_OK;
    }
    if (!IsValidEventId(event_id)) {
        (void)Det_ReportError(DET_MODULE_ID_DEM, 0U, DEM_API_ID_SET_EVENT_STATUS,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    if ((status != DEM_EVENT_STATUS_PASSED) && (status != DEM_EVENT_STATUS_FAILED)) {
        (void)Det_ReportError(DET_MODULE_ID_DEM, 0U, DEM_API_ID_SET_EVENT_STATUS,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }

    Dem_EventRecordType *rec = &g_events[event_id];
    Gpt_ValueType now = Gpt_GetTimeElapsed(GPT_CHANNEL_TIME_BASE);

    if (status == DEM_EVENT_STATUS_FAILED) {
        if (rec->currentStatus != DEM_EVENT_STATUS_FAILED) {
            if (rec->occurrenceCount < UINT32_MAX) {
                rec->occurrenceCount++;
            }
        }
        rec->currentStatus = DEM_EVENT_STATUS_FAILED;
        rec->lastFailedTimestampMs = now;
    } else {
        rec->currentStatus = DEM_EVENT_STATUS_PASSED;
        rec->lastPassedTimestampMs = now;
    }
    rec->everReported = TRUE;

    return E_OK;
}

void Dem_MainFunction(void) {
    /* Phase 1: no periodic aggregation logic yet. Present so the public
     * API surface matches spec section 7.4 ahead of the phases that will
     * actually set events from real detection logic. */
}

Std_ReturnType Dem_GetEventRecord(Dem_EventIdType event_id, Dem_EventRecordType *out) {
    if (out == NULL) {
        return E_NOT_OK;
    }
    if (!IsValidEventId(event_id)) {
        return E_NOT_OK;
    }
    *out = g_events[event_id];
    return E_OK;
}
