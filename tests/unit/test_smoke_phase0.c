/**
 * @file test_smoke_phase0.c
 * @brief Phase 0 CTest smoke test.
 *
 * Not a real unit test yet — the modules to test don't exist until later
 * phases (see tests/unit coverage table in
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 14.1). This only proves
 * that `platform` links and its type widths/values are what the rest of
 * the specification assumes, so `ctest` has something meaningful to run
 * starting in Phase 0.
 */
#include <stdio.h>

#include "ComStack_Types.h"
#include "Platform_Types.h"
#include "Std_Types.h"

static int check(int condition, const char *description) {
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
    }
    return condition ? 0 : 1;
}

int main(void) {
    int failures = 0;

    failures += check(sizeof(uint8) == 1U, "uint8 is 1 byte");
    failures += check(sizeof(uint16) == 2U, "uint16 is 2 bytes");
    failures += check(sizeof(uint32) == 4U, "uint32 is 4 bytes");
    failures += check(TRUE == 1U, "TRUE == 1");
    failures += check(FALSE == 0U, "FALSE == 0");
    failures += check(E_OK == 0U, "E_OK == 0");
    failures += check(E_NOT_OK == 1U, "E_NOT_OK == 1");
    failures += check(sizeof(PduIdType) == 2U, "PduIdType is 2 bytes");

    if (failures == 0) {
        (void)printf("test_smoke_phase0: all checks passed\n");
        return 0;
    }

    (void)fprintf(stderr, "test_smoke_phase0: %d check(s) failed\n", failures);
    return 1;
}
