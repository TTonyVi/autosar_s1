#include "Det.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static Det_RecordType g_ring[DET_MAX_RECORDS];
static uint32 g_ringHead;
static uint32 g_ringCount;
static uint32 g_totalErrorCount;
static uint32 g_moduleErrorCount[DET_MAX_MODULE_ID];
static boolean g_initialized;

void Det_Init(void) {
    g_ringHead = 0U;
    g_ringCount = 0U;
    g_totalErrorCount = 0U;
    (void)memset(g_moduleErrorCount, 0, sizeof(g_moduleErrorCount));
    (void)memset(g_ring, 0, sizeof(g_ring));
    g_initialized = TRUE;
}

void Det_DeInit(void) {
    /* Idempotent per REQ-ECUM-004: calling this again, or before Init, is
     * safe. State is intentionally left as-is (not cleared) so a crash
     * dump taken right after DeInit can still show the last records. */
    g_initialized = FALSE;
}

Std_ReturnType Det_ReportError(Det_ModuleIdType module_id, Det_InstanceIdType instance_id,
                               Det_ApiIdType api_id, Det_ErrorIdType error_id) {
    if (!g_initialized) {
        return E_NOT_OK;
    }

    /* Bounded, saturating counters — no dynamic growth, never wraps back
     * to a misleadingly small value (spec section 10.1: "SHALL not
     * allocate memory"). */
    if (g_totalErrorCount < UINT32_MAX) {
        g_totalErrorCount++;
    }
    if (module_id < DET_MAX_MODULE_ID) {
        if (g_moduleErrorCount[module_id] < UINT32_MAX) {
            g_moduleErrorCount[module_id]++;
        }
    }

    Det_RecordType *slot = &g_ring[g_ringHead];
    slot->moduleId = module_id;
    slot->instanceId = instance_id;
    slot->apiId = api_id;
    slot->errorId = error_id;

    g_ringHead = (g_ringHead + 1U) % DET_MAX_RECORDS;
    if (g_ringCount < DET_MAX_RECORDS) {
        g_ringCount++;
    }

    return E_OK;
}

uint32 Det_GetTotalErrorCount(void) {
    return g_totalErrorCount;
}

uint32 Det_GetModuleErrorCount(Det_ModuleIdType module_id) {
    if (module_id >= DET_MAX_MODULE_ID) {
        return 0U;
    }
    return g_moduleErrorCount[module_id];
}

uint32 Det_GetRecordCount(void) {
    return g_ringCount;
}

Std_ReturnType Det_GetRecord(uint32 indexFromOldest, Det_RecordType *out) {
    if (out == NULL) {
        return E_NOT_OK;
    }
    if (indexFromOldest >= g_ringCount) {
        return E_NOT_OK;
    }

    /* Before the ring has wrapped, the oldest record is always at index 0;
     * once it saturates, g_ringHead points at the next slot to overwrite,
     * which is also the oldest surviving record. */
    uint32 oldestIndex = (g_ringCount < DET_MAX_RECORDS) ? 0U : g_ringHead;
    uint32 actualIndex = (oldestIndex + indexFromOldest) % DET_MAX_RECORDS;
    *out = g_ring[actualIndex];
    return E_OK;
}
