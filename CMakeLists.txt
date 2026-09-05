cmake_minimum_required(VERSION 3.20)
project(autosar_speed_monitor C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)

enable_testing()

# ---------------------------------------------------------------------------
# platform: fixed-width base types shared by every layer (section 7.3, 12.2)
# ---------------------------------------------------------------------------
add_library(platform INTERFACE)
target_include_directories(platform INTERFACE ${CMAKE_SOURCE_DIR}/platform/include)

# ---------------------------------------------------------------------------
# BSW libraries (section 12, 12.2)
#
# Real module sources are added directory-by-directory as the corresponding
# implementation phase (see section 16) lands; the GLOB-free explicit
# source lists below are extended at that point rather than switched to a
# wildcard, so every source file that joins a target is a reviewed,
# intentional change. Phase 1 adds Mcu/Port/Gpt (bsw_mcal) and Det/Dem/Os
# (bsw_services); the matching Phase 0 placeholder sources are removed.
# ---------------------------------------------------------------------------
add_library(bsw_services STATIC
    bsw/services/Det/Det.c
    bsw/services/Dem/Dem.c
    bsw/os/Os/Os.c
)
target_link_libraries(bsw_services PUBLIC platform)
target_include_directories(bsw_services PUBLIC
    bsw/services/Det
    bsw/services/Dem
    bsw/services/EcuM
    bsw/os/Os
    bsw/mcal/Gpt # Dem/Os derive all timing from Gpt (section 8.1); header-only dependency, resolved at final link
)
autosar_apply_warnings(bsw_services)

add_library(bsw_com STATIC
    bsw/communication/Bsw_Com_Placeholder.c
)
target_link_libraries(bsw_com PUBLIC platform)
target_include_directories(bsw_com PUBLIC
    bsw/communication/Com
    bsw/communication/PduR
    bsw/communication/CanIf
)
autosar_apply_warnings(bsw_com)

add_library(bsw_mcal STATIC
    bsw/mcal/Mcu/Mcu.c
    bsw/mcal/Port/Port.c
    bsw/mcal/Gpt/Gpt.c
    bsw/mcal/VirtualHw/VirtualHw.c
    bsw/mcal/VirtualHw/VirtualHw_Parse.c
    bsw/mcal/Adc/Adc.c
    bsw/mcal/Dio/Dio.c
    bsw/mcal/Display/Display.c
)
target_link_libraries(bsw_mcal PUBLIC platform)
target_include_directories(bsw_mcal PUBLIC
    bsw/mcal/Mcu
    bsw/mcal/Port
    bsw/mcal/Gpt
    bsw/mcal/Adc
    bsw/mcal/Dio
    bsw/mcal/Display
    bsw/mcal/Can
    bsw/mcal/VirtualHw
    bsw/services/Det # Mcu/Port/Gpt report to Det; header-only dependency, resolved at final link
)
autosar_apply_warnings(bsw_mcal)

add_library(bsw_ecuab STATIC
    bsw/ecu_abstraction/Bsw_EcuAb_Placeholder.c
)
target_link_libraries(bsw_ecuab PUBLIC platform)
target_include_directories(bsw_ecuab PUBLIC
    bsw/ecu_abstraction/IoHwAb
)
autosar_apply_warnings(bsw_ecuab)

# ---------------------------------------------------------------------------
# RTE libraries: ECU-specific generated-style code (section 5, REQ-RTE-007)
# ---------------------------------------------------------------------------
add_library(rte_sensor STATIC
    rte/generated/SensorEcu/Rte_Sensor_Placeholder.c
)
target_link_libraries(rte_sensor PUBLIC platform)
target_include_directories(rte_sensor PUBLIC
    rte/common
    rte/generated/SensorEcu
)
autosar_apply_warnings(rte_sensor)

add_library(rte_cluster STATIC
    rte/generated/ClusterEcu/Rte_Cluster_Placeholder.c
)
target_link_libraries(rte_cluster PUBLIC platform)
target_include_directories(rte_cluster PUBLIC
    rte/common
    rte/generated/ClusterEcu
)
autosar_apply_warnings(rte_cluster)

# ---------------------------------------------------------------------------
# ASW libraries: application SW-Cs (section 4, REQ-ARCH-001/002)
# ---------------------------------------------------------------------------
add_library(asw_sensor STATIC
    asw/VehicleStateSwc/Asw_Sensor_Placeholder.c
)
target_link_libraries(asw_sensor PUBLIC platform)
autosar_apply_warnings(asw_sensor)

add_library(asw_cluster STATIC
    asw/ClusterControlSwc/Asw_Cluster_Placeholder.c
)
target_link_libraries(asw_cluster PUBLIC platform)
autosar_apply_warnings(asw_cluster)

# ---------------------------------------------------------------------------
# ECU executables (section 12.2)
# ---------------------------------------------------------------------------
add_executable(sensor_ecu
    ecu/SensorEcu/main.c
)
target_link_libraries(sensor_ecu PRIVATE
    asw_sensor
    rte_sensor
    bsw_ecuab
    bsw_com
    bsw_mcal
    bsw_services
    platform
)
autosar_apply_warnings(sensor_ecu)

add_executable(cluster_ecu
    ecu/ClusterEcu/main.c
)
target_link_libraries(cluster_ecu PRIVATE
    asw_cluster
    rte_cluster
    bsw_ecuab
    bsw_com
    bsw_mcal
    bsw_services
    platform
)
autosar_apply_warnings(cluster_ecu)

# ---------------------------------------------------------------------------
# Tests (section 14). Real unit test executables (test_<module>) are added
# as each module lands; this Phase 0 smoke test only proves the build/test
# harness works end to end.
# ---------------------------------------------------------------------------
add_executable(test_smoke_phase0
    tests/unit/test_smoke_phase0.c
)
target_link_libraries(test_smoke_phase0 PRIVATE platform)
autosar_apply_warnings(test_smoke_phase0)
add_test(NAME test_smoke_phase0 COMMAND test_smoke_phase0)

# ---------------------------------------------------------------------------
# Phase 1 unit tests: Os and Det/Dem, built against a fake Gpt instead of
# the real bsw_mcal one, so scheduler/timeout behavior is deterministic
# regardless of host speed (section 16, Phase 1; section 14.1). The fake's
# .c file is compiled directly into each test executable (not through a
# library) so its symbols always win over any real Gpt — these tests never
# link bsw_mcal.
# ---------------------------------------------------------------------------
add_executable(test_scheduler
    tests/unit/test_scheduler.c
    tests/fixtures/Gpt_Fake.c
)
target_include_directories(test_scheduler PRIVATE tests/fixtures)
target_link_libraries(test_scheduler PRIVATE bsw_services platform)
autosar_apply_warnings(test_scheduler)
add_test(NAME test_scheduler COMMAND test_scheduler)

add_executable(test_det_dem
    tests/unit/test_det_dem.c
    tests/fixtures/Gpt_Fake.c
)
target_include_directories(test_det_dem PRIVATE tests/fixtures)
target_link_libraries(test_det_dem PRIVATE bsw_services platform)
autosar_apply_warnings(test_det_dem)
add_test(NAME test_det_dem COMMAND test_det_dem)

# ---------------------------------------------------------------------------
# Phase 2 unit tests: VirtualHw, Adc/Dio, Display (section 16, Phase 2;
# section 14.1).
#
# test_virtual_hw exercises the REAL VirtualHw.c (real AF_UNIX sockets) plus
# the REAL Gpt.c — this is the layer that owns the socket, so it must be
# tested against real socket behavior; none of its assertions need
# controlled time. It never links bsw_mcal (which would duplicate these
# same .c files via the archive); instead the needed real sources are
# compiled directly into the test executable, same seam as Phase 1.
#
# test_adc_dio and test_display exercise the REAL Adc.c/Dio.c/Display.c but
# against the FAKE VirtualHw and FAKE Gpt (tests/fixtures), for full,
# deterministic control over "the latest payload" and "how much time has
# passed" — exactly the section 16 Phase 2 deliverable ("unit tests using a
# fake link"). VirtualHw_Parse.c has no sockets or state, so the real one is
# reused unmodified alongside the fake.
# ---------------------------------------------------------------------------
add_executable(test_virtual_hw
    tests/unit/test_virtual_hw.c
    bsw/mcal/VirtualHw/VirtualHw.c
    bsw/mcal/VirtualHw/VirtualHw_Parse.c
    bsw/mcal/Gpt/Gpt.c
)
target_include_directories(test_virtual_hw PRIVATE bsw/mcal/VirtualHw bsw/mcal/Gpt)
target_link_libraries(test_virtual_hw PRIVATE bsw_services platform)
autosar_apply_warnings(test_virtual_hw)
add_test(NAME test_virtual_hw COMMAND test_virtual_hw)

add_executable(test_adc_dio
    tests/unit/test_adc_dio.c
    bsw/mcal/Adc/Adc.c
    bsw/mcal/Dio/Dio.c
    bsw/mcal/VirtualHw/VirtualHw_Parse.c
    tests/fixtures/VirtualHw_Fake.c
    tests/fixtures/Gpt_Fake.c
)
target_include_directories(test_adc_dio PRIVATE
    bsw/mcal/Adc
    bsw/mcal/Dio
    bsw/mcal/VirtualHw
    bsw/mcal/Gpt
    tests/fixtures
)
target_link_libraries(test_adc_dio PRIVATE bsw_services platform)
autosar_apply_warnings(test_adc_dio)
add_test(NAME test_adc_dio COMMAND test_adc_dio)

add_executable(test_display
    tests/unit/test_display.c
    bsw/mcal/Display/Display.c
    bsw/mcal/VirtualHw/VirtualHw_Parse.c
    tests/fixtures/VirtualHw_Fake.c
    tests/fixtures/Gpt_Fake.c
)
target_include_directories(test_display PRIVATE
    bsw/mcal/Display
    bsw/mcal/VirtualHw
    bsw/mcal/Gpt
    tests/fixtures
)
target_link_libraries(test_display PRIVATE bsw_services platform)
autosar_apply_warnings(test_display)
add_test(NAME test_display COMMAND test_display)
