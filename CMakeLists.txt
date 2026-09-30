cmake_minimum_required(VERSION 3.20)
project(autosar_speed_monitor C)

# This build is S32K144-hardware-only (see AUTOSAR_CLASSIC_PROJECT_SPEC_v3.0.md,
# section 0.3's v3.0 changelog entry): it must be configured with
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/s32k144_toolchain.cmake
# There is no host/native build path in this tree any more, and no unit
# tests (verification is by flashing real hardware — see the spec's
# hardware bring-up checklist). The previous Ubuntu-simulation build,
# with its full unit/integration test suite, is preserved standalone under
# legacy-sim/ (its own CMakeLists.txt, untouched).
if(NOT CMAKE_CROSSCOMPILING)
    message(WARNING
        "Configuring without -DCMAKE_TOOLCHAIN_FILE=cmake/s32k144_toolchain.cmake — "
        "this tree only builds S32K144 firmware; see the warning above the project() call.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# ---------------------------------------------------------------------------
# platform: fixed-width base types shared by every layer
# ---------------------------------------------------------------------------
add_library(platform INTERFACE)
target_include_directories(platform INTERFACE ${CMAKE_SOURCE_DIR}/platform/include)

# ---------------------------------------------------------------------------
# BSW libraries
# ---------------------------------------------------------------------------
add_library(bsw_services STATIC
    bsw/services/Det/Det.c
    bsw/services/Dem/Dem.c
    bsw/services/Logger/Logger.c
    bsw/services/EcuM/EcuM.c
    bsw/services/BswM/BswM.c
    bsw/services/ComM/ComM.c
    bsw/os/Os/Os.c
    bsw/os/Os/Os_Counter.c
)
# The BSW layers are mutually dependent by design, and declaring it is what
# lets CMake repeat the archives on the link line instead of failing:
# bsw_mcal reports to Det, Dem/Logger read Gpt, and EcuM (in bsw_services)
# owns the init order so it calls into every layer. Cycles between STATIC
# libraries are supported by CMake precisely for this case.
target_link_libraries(bsw_services PUBLIC platform bsw_mcal bsw_com bsw_ecuab)
target_include_directories(bsw_services PUBLIC
    bsw/services/Det
    bsw/services/Dem
    bsw/services/Logger
    bsw/services/EcuM
    bsw/services/BswM
    bsw/services/ComM
    bsw/communication/CanSM
    bsw/os/Os
    # EcuM owns the whole init order (section 9.2), so it is the one module
    # that legitimately sees every other module's header. These are
    # header-only dependencies, resolved when an ECU executable links the
    # corresponding libraries.
    bsw/mcal/Mcu
    bsw/mcal/Port
    bsw/mcal/Adc
    bsw/mcal/Dio
    bsw/mcal/Display
    bsw/mcal/Can
    bsw/ecu_abstraction/IoHwAb
    bsw/communication/CanIf
    bsw/communication/PduR
    bsw/communication/Com
    rte/common
    bsw/mcal/Gpt # Dem derives its timestamps from Gpt (section 8.1); header-only dependency, resolved at final link
    bsw/mcal/SysTimer # Os_Counter reads the OS's OWN timer, never Gpt (section 8.1); header-only, resolved at final link
)
autosar_apply_warnings(bsw_services)

add_library(bsw_com STATIC
    bsw/communication/Com/Com.c
    bsw/communication/Com/ComProtection.c
    bsw/communication/PduR/PduR.c
    bsw/communication/CanIf/CanIf.c
    bsw/communication/CanSM/CanSM.c
)
target_link_libraries(bsw_com PUBLIC platform bsw_mcal bsw_services)
target_include_directories(bsw_com PUBLIC
    bsw/communication/Com
    bsw/communication/PduR
    bsw/communication/CanIf
    bsw/communication/CanSM
    bsw/services/ComM
    bsw/mcal/Can # CanIf.h depends on Can.h (Can_ControllerStateType, Can_HwHandleType);
                 # header-only dependency, resolved at final link (same seam as bsw_mcal -> Det)
    bsw/mcal/Gpt # Com.h derives all timing from Gpt (section 8.1); header-only dependency,
                 # resolved at final link
    bsw/services/Det # CanIf/PduR/Com report to Det; header-only dependency, resolved at final link
)
autosar_apply_warnings(bsw_com)

# One fixed S32K144 register-level source list — there is only one MCAL
# variant in this tree (see the note at the top of this file).
add_library(bsw_mcal STATIC
    bsw/mcal/Mcu/Mcu_S32K144.c
    bsw/mcal/Port/Port_S32K144.c
    bsw/mcal/Gpt/Gpt_S32K144.c
    bsw/mcal/SysTimer/SysTimer_S32K144.c
    bsw/mcal/SysTimer/SysTimer_Cfg.c
    bsw/mcal/Adc/Adc_S32K144.c
    bsw/mcal/Dio/Dio_S32K144.c
    bsw/mcal/Display/Display_S32K144.c
    bsw/mcal/Can/Can_S32K144.c
    platform/s32k144/startup_S32K144.c
)
target_link_libraries(bsw_mcal PUBLIC platform bsw_services bsw_com)
target_include_directories(bsw_mcal PUBLIC
    bsw/mcal/Mcu
    bsw/mcal/Port
    bsw/mcal/Gpt
    bsw/mcal/SysTimer
    bsw/mcal/Adc
    bsw/mcal/Dio
    bsw/mcal/Display
    bsw/mcal/Can
    platform/s32k144
    bsw/services/Det # Mcu/Port/Gpt/SysTimer/Can/Display report to Det; header-only dependency, resolved at final link
    bsw/communication/CanIf # Can.c calls CanIf_RxIndication/CanIf_TxConfirmation (section 7.1
                             # rule 9, section 7.2); header-only dependency, resolved at final
                             # link when an ECU executable links both bsw_mcal and bsw_com
)
autosar_apply_warnings(bsw_mcal)

add_library(bsw_ecuab STATIC
    bsw/ecu_abstraction/IoHwAb/IoHwAb.c
    bsw/ecu_abstraction/IoHwAb/IoHwAb_Cfg.c
)
target_link_libraries(bsw_ecuab PUBLIC platform bsw_mcal bsw_services)
target_include_directories(bsw_ecuab PUBLIC
    bsw/ecu_abstraction/IoHwAb
    rte/common       # application data types (section 4.3); no RTE code, types only
    bsw/mcal/Adc     # IoHwAb reads the input drivers (section 3.1 "May call")
    bsw/mcal/Dio
    bsw/mcal/Display
    bsw/mcal/Gpt     # input staleness and the display frame timestamp
    bsw/services/Det # header-only dependency, resolved at final link
    bsw/services/Dem
)
autosar_apply_warnings(bsw_ecuab)

# ---------------------------------------------------------------------------
# RTE libraries: ECU-specific generated-style code (section 5, REQ-RTE-007)
# ---------------------------------------------------------------------------
add_library(rte_sensor STATIC
    rte/generated/SensorEcu/Rte_SensorEcu.c
)
target_link_libraries(rte_sensor PUBLIC platform)
target_include_directories(rte_sensor PUBLIC
    rte/common
    rte/generated/SensorEcu
    asw/VehicleStateSwc          # the RTE invokes the SW-C's runnables
    bsw/ecu_abstraction/IoHwAb   # client port adapter (section 5.1)
    bsw/communication/Com        # REQ-RTE-004 signal mapping
    bsw/services/Dem             # section 10.2 COM-layer DEM events, set in UpdateReceptionHealth
    bsw/mcal/Gpt                 # reached only through Com.h's own timing types
    bsw/services/Det
)
autosar_apply_warnings(rte_sensor)

add_library(rte_cluster STATIC
    rte/generated/ClusterEcu/Rte_ClusterEcu.c
)
target_link_libraries(rte_cluster PUBLIC platform)
target_include_directories(rte_cluster PUBLIC
    rte/common
    rte/generated/ClusterEcu
    asw/ClusterControlSwc
    bsw/ecu_abstraction/IoHwAb
    bsw/communication/Com
    bsw/services/Dem # section 10.2 COM-layer DEM events, set in UpdateReceptionHealth
    bsw/mcal/Gpt # reached only through Com.h's own timing types
    bsw/services/Det
)
autosar_apply_warnings(rte_cluster)

# ---------------------------------------------------------------------------
# ASW libraries: application SW-Cs (section 4, REQ-ARCH-001/002)
# ---------------------------------------------------------------------------
# A SW-C library's include path deliberately contains ONLY its own
# directory, rte/common, and its generated RTE header directory. It cannot
# reach a BSW or MCAL header even by accident (REQ-ARCH-001/002), which is
# the build-level half of what the static check proves at review time.
add_library(asw_sensor STATIC
    asw/VehicleStateSwc/VehicleStateSwc.c
)
target_link_libraries(asw_sensor PUBLIC platform)
target_include_directories(asw_sensor PUBLIC
    asw/VehicleStateSwc
    rte/common
    rte/generated/SensorEcu
)
autosar_apply_warnings(asw_sensor)

add_library(asw_cluster STATIC
    asw/ClusterControlSwc/ClusterControlSwc.c
)
target_link_libraries(asw_cluster PUBLIC platform)
target_include_directories(asw_cluster PUBLIC
    asw/ClusterControlSwc
    rte/common
    rte/generated/ClusterEcu
)
autosar_apply_warnings(asw_cluster)

# ---------------------------------------------------------------------------
# ECU firmware images (section 12.2)
# ---------------------------------------------------------------------------
function(autosar_add_ecu_image name main_source config_source config_dir rte_lib asw_lib)
    add_executable(${name} ${main_source} ${config_source})
    target_include_directories(${name} PRIVATE ${config_dir})
    # bsw_services comes first because EcuM lives there and calls Rte_Start,
    # which the ECU-specific RTE provides; the BSW cycle declared above
    # makes the archives repeat for the calls that go the other way.
    target_link_libraries(${name} PRIVATE
        bsw_services
        ${rte_lib}
        ${asw_lib}
        bsw_ecuab
        bsw_com
        bsw_mcal
        platform
    )
    autosar_apply_warnings(${name})

    # Emit a flashable .hex plus a size summary; CMAKE_OBJCOPY/CMAKE_SIZE_UTIL
    # are set by cmake/s32k144_toolchain.cmake.
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${name}> $<TARGET_FILE:${name}>.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> $<TARGET_FILE:${name}>.bin
        COMMAND ${CMAKE_SIZE_UTIL} $<TARGET_FILE:${name}>
        COMMENT "Generating ${name}.hex/.bin"
    )
endfunction()

autosar_add_ecu_image(sensor_ecu
    ecu/SensorEcu/main.c ecu/SensorEcu/config/SensorEcu_Cfg.c ecu/SensorEcu/config
    rte_sensor asw_sensor
)

autosar_add_ecu_image(cluster_ecu
    ecu/ClusterEcu/main.c ecu/ClusterEcu/config/ClusterEcu_Cfg.c ecu/ClusterEcu/config
    rte_cluster asw_cluster
)
