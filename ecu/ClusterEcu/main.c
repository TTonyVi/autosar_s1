/**
 * @file main.c
 * @brief ClusterEcu process entry point.
 *
 * Phase 0 placeholder: prints identity/version and exits successfully.
 * Command-line parsing, EcuM initialization order, and the real scheduler
 * loop are introduced starting in later phases (see
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 9 and 15.4).
 */
#include <stdio.h>

#include "Platform_Types.h"

#define CLUSTER_ECU_VERSION_STRING "0.0.0-phase0"

int main(void) {
    (void)printf("ClusterEcu placeholder build, version %s\n", CLUSTER_ECU_VERSION_STRING);
    return 0;
}
