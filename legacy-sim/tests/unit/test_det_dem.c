/**
 * @file test_det_dem.c
 * @brief Phase 1 unit tests for Det (bsw/services/Det) and Dem
 * (bsw/services/Dem).
 *
 * Covers the mandatory coverage from AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md
 * section 14.1 ("test_det_dem"): bounded counters/ring buffer for Det, and
 * failed/passed event transitions for Dem. Uses the fake Gpt (tests/
 * fixtures) to control Dem's timestamps deterministically.
 */
#include <stdio.h>

#include "Dem.h"
#include "Det.h"
#include "Gpt_Fake.h"
#include "Platform_Types.h"

static int g_failures;

static void Check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        g_failures++;
    }
}

static void TestDetBoundedRingBuffer(void) {
    Det_Init();

    const uint32 totalReports = 70U; /* > DET_MAX_RECORDS (64) on purpose */
    for (uint32 i = 0U; i < totalReports; i++) {
        (void)Det_ReportError(DET_MODULE_ID_OS, 0U, (Det_ApiIdType)i, DET_ERR_SCHEDULER_OVERRUN);
    }

    Check(Det_GetTotalErrorCount() == totalReports,
          "total error count is not bounded, counts every report");
    Check(Det_GetRecordCount() == DET_MAX_RECORDS,
          "ring buffer saturates at DET_MAX_RECORDS (64), does not grow further");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_OS) == totalReports,
          "per-module counter counts every report for that module");

    /* The oldest 6 reports (api_id 0..5) were pushed out of the 64-slot
     * ring; the oldest surviving one is report #6. */
    Det_RecordType rec;
    Std_ReturnType r = Det_GetRecord(0U, &rec);
    Check(r == E_OK, "Det_GetRecord succeeds for the oldest surviving record");
    Check(rec.apiId == 6U, "the oldest surviving ring entry is report #6 (0..5 were pushed out)");

    r = Det_GetRecord(DET_MAX_RECORDS - 1U, &rec);
    Check(r == E_OK, "Det_GetRecord succeeds for the newest record");
    Check(rec.apiId == 69U, "the newest ring entry is the very last report made");

    r = Det_GetRecord(DET_MAX_RECORDS, &rec);
    Check(r == E_NOT_OK, "Det_GetRecord rejects an out-of-range index");
}

static void TestDetPerModuleCountersIndependent(void) {
    Det_Init();

    (void)Det_ReportError(DET_MODULE_ID_GPT, 0U, 0U, DET_ERR_UNINIT);
    (void)Det_ReportError(DET_MODULE_ID_GPT, 0U, 0U, DET_ERR_UNINIT);
    (void)Det_ReportError(DET_MODULE_ID_MCU, 0U, 0U, DET_ERR_NULL_POINTER);

    Check(Det_GetModuleErrorCount(DET_MODULE_ID_GPT) == 2U,
          "Gpt's counter reflects only Gpt's reports");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_MCU) == 1U,
          "Mcu's counter reflects only Mcu's reports");
    Check(Det_GetModuleErrorCount(DET_MODULE_ID_OS) == 0U,
          "a module that never reported has a zero counter");
    Check(Det_GetTotalErrorCount() == 3U, "the total counter sums every module's reports");
}

static void TestDemFailedPassedTransitions(void) {
    GptFake_Reset();
    Det_Init();
    Dem_Init();

    GptFake_SetTimeMs(100U);
    Std_ReturnType r = Dem_SetEventStatus(DEM_EVENT_SENSOR_LINK_LOST, DEM_EVENT_STATUS_FAILED);
    Check(r == E_OK, "Dem_SetEventStatus(FAILED) is accepted for a valid event");

    Dem_EventRecordType rec;
    r = Dem_GetEventRecord(DEM_EVENT_SENSOR_LINK_LOST, &rec);
    Check(r == E_OK, "Dem_GetEventRecord succeeds for a valid event");
    Check(rec.currentStatus == DEM_EVENT_STATUS_FAILED,
          "event is FAILED right after being set failed");
    Check(rec.occurrenceCount == 1U, "first transition to failed counts as one occurrence");
    Check(rec.lastFailedTimestampMs == 100U,
          "failed timestamp matches the (fake) clock at the time it failed");

    /* Repeating FAILED while already failed refreshes the timestamp but is
     * not a new occurrence — a project-local simplification documented in
     * Dem.h. */
    GptFake_SetTimeMs(150U);
    (void)Dem_SetEventStatus(DEM_EVENT_SENSOR_LINK_LOST, DEM_EVENT_STATUS_FAILED);
    (void)Dem_GetEventRecord(DEM_EVENT_SENSOR_LINK_LOST, &rec);
    Check(rec.occurrenceCount == 1U,
          "repeating FAILED while already failed does not add a new occurrence");
    Check(rec.lastFailedTimestampMs == 150U,
          "failed timestamp still refreshes on a repeated failure report");

    GptFake_SetTimeMs(200U);
    (void)Dem_SetEventStatus(DEM_EVENT_SENSOR_LINK_LOST, DEM_EVENT_STATUS_PASSED);
    (void)Dem_GetEventRecord(DEM_EVENT_SENSOR_LINK_LOST, &rec);
    Check(rec.currentStatus == DEM_EVENT_STATUS_PASSED, "event transitions to PASSED");
    Check(rec.lastPassedTimestampMs == 200U,
          "passed timestamp matches the (fake) clock at the time it passed");
    Check(rec.occurrenceCount == 1U, "passing does not change the occurrence count");

    GptFake_SetTimeMs(300U);
    (void)Dem_SetEventStatus(DEM_EVENT_SENSOR_LINK_LOST, DEM_EVENT_STATUS_FAILED);
    (void)Dem_GetEventRecord(DEM_EVENT_SENSOR_LINK_LOST, &rec);
    Check(rec.occurrenceCount == 2U,
          "a second passed-to-failed transition counts as a second occurrence");
}

static void TestDemRejectsInvalidInput(void) {
    GptFake_Reset();
    Det_Init();
    Dem_Init();

    uint32 detBefore = Det_GetTotalErrorCount();

    Std_ReturnType r = Dem_SetEventStatus((Dem_EventIdType)0U, DEM_EVENT_STATUS_FAILED);
    Check(r == E_NOT_OK, "event ID 0 (reserved/invalid) is rejected");
    Check(Det_GetTotalErrorCount() == detBefore + 1U, "an invalid event ID is reported to Det");

    r = Dem_SetEventStatus(DEM_EVENT_SENSOR_LINK_LOST, (Dem_EventStatusType)99U);
    Check(r == E_NOT_OK, "an invalid status value is rejected");
    Check(Det_GetTotalErrorCount() == detBefore + 2U, "an invalid status value is reported to Det");
}

int main(void) {
    TestDetBoundedRingBuffer();
    TestDetPerModuleCountersIndependent();
    TestDemFailedPassedTransitions();
    TestDemRejectsInvalidInput();

    if (g_failures == 0) {
        (void)printf("test_det_dem: all checks passed\n");
        return 0;
    }
    (void)fprintf(stderr, "test_det_dem: %d check(s) failed\n", g_failures);
    return 1;
}
