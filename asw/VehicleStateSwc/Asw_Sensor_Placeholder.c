/**
 * @file Asw_Sensor_Placeholder.c
 * @brief Phase 0 placeholder translation unit for the `asw_sensor` target.
 *
 * Real content (`VehicleStateSwc` runnables) lands in Phase 5. Per
 * REQ-ARCH-001/002, application code SHALL include only its matching
 * `Rte_<Swc>.h`, application-local headers, and standard fixed-width
 * headers — this placeholder already respects that constraint by including
 * only `Platform_Types.h`.
 */
#include "Platform_Types.h"

uint8 AswSensor_PlaceholderVersion(void) {
    return 0U;
}
