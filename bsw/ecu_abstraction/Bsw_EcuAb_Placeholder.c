/**
 * @file Bsw_EcuAb_Placeholder.c
 * @brief Phase 0 placeholder translation unit for the `bsw_ecuab` target.
 *
 * Real content (IoHwAb) lands in Phase 5. This file only exists so the
 * CMake target has a compilable source and the target name from
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 12.2, is already wired
 * into the build graph.
 */
#include "Platform_Types.h"

uint8 BswEcuAb_PlaceholderVersion(void) {
    return 0U;
}
