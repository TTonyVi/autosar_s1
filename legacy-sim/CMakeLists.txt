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
    bsw/mcal/VirtualHw
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

# REQ-ARCH-012: "Replacing the Ubuntu MCAL with a microcontroller MCAL
# SHALL require no change in asw/, rte/, or any bsw/ directory other than
# bsw/mcal/." This switch is that claim's build-level proof: the "stub"
# variant swaps only the two files that touch a host socket
# (bsw/mcal/Can/Can_Stub.c, bsw/mcal/VirtualHw/VirtualHw_Stub.c) for
# in-memory-buffer equivalents of the exact same headers -- every other
# source in this list, and every file in bsw_com/bsw_ecuab/bsw_services/
# rte/asw/ecu, is untouched either way. Verify with:
#   cmake -S . -B build-stubmcal -DAUTOSAR_MCAL_VARIANT=stub
#   cmake --build build-stubmcal --parallel
# (see docs/IMPLEMENTATION_STATUS.md, Phase 8, for the verification log).
set(AUTOSAR_MCAL_VARIANT "real" CACHE STRING
    "MCAL variant: 'real' (SocketCAN/AF_UNIX, the normal build), 'stub' (in-memory, REQ-ARCH-012 proof), or 's32k144' (bare-metal S32K144)")
set_property(CACHE AUTOSAR_MCAL_VARIANT PROPERTY STRINGS real stub s32k144)

if(AUTOSAR_MCAL_VARIANT STREQUAL "stub")
    set(AUTOSAR_CAN_SOURCE bsw/mcal/Can/Can_Stub.c)
    set(AUTOSAR_VIRTUALHW_SOURCE bsw/mcal/VirtualHw/VirtualHw_Stub.c)
    set(AUTOSAR_MCU_SOURCE bsw/mcal/Mcu/Mcu.c)
    set(AUTOSAR_PORT_SOURCE bsw/mcal/Port/Port.c)
    set(AUTOSAR_GPT_SOURCE bsw/mcal/Gpt/Gpt.c)
    set(AUTOSAR_SYSTIMER_SOURCE bsw/mcal/SysTimer/SysTimer.c)
    set(AUTOSAR_ADC_SOURCE bsw/mcal/Adc/Adc.c)
    set(AUTOSAR_DIO_SOURCE bsw/mcal/Dio/Dio.c)
    set(AUTOSAR_STARTUP_SOURCE "")
elseif(AUTOSAR_MCAL_VARIANT STREQUAL "s32k144")
    set(AUTOSAR_CAN_SOURCE bsw/mcal/Can/Can_S32K144.c)
    set(AUTOSAR_VIRTUALHW_SOURCE bsw/mcal/VirtualHw/VirtualHw_Stub.c)
    set(AUTOSAR_MCU_SOURCE bsw/mcal/Mcu/Mcu_S32K144.c)
    set(AUTOSAR_PORT_SOURCE bsw/mcal/Port/Port_S32K144.c)
    set(AUTOSAR_GPT_SOURCE bsw/mcal/Gpt/Gpt_S32K144.c)
    set(AUTOSAR_SYSTIMER_SOURCE bsw/mcal/SysTimer/SysTimer_S32K144.c)
    set(AUTOSAR_ADC_SOURCE bsw/mcal/Adc/Adc_S32K144.c)
    set(AUTOSAR_DIO_SOURCE bsw/mcal/Dio/Dio_S32K144.c)
    set(AUTOSAR_STARTUP_SOURCE platform/s32k144/startup_S32K144.c)
else()
    set(AUTOSAR_CAN_SOURCE bsw/mcal/Can/Can.c)
    set(AUTOSAR_VIRTUALHW_SOURCE bsw/mcal/VirtualHw/VirtualHw.c)
    set(AUTOSAR_MCU_SOURCE bsw/mcal/Mcu/Mcu.c)
    set(AUTOSAR_PORT_SOURCE bsw/mcal/Port/Port.c)
    set(AUTOSAR_GPT_SOURCE bsw/mcal/Gpt/Gpt.c)
    set(AUTOSAR_SYSTIMER_SOURCE bsw/mcal/SysTimer/SysTimer.c)
    set(AUTOSAR_ADC_SOURCE bsw/mcal/Adc/Adc.c)
    set(AUTOSAR_DIO_SOURCE bsw/mcal/Dio/Dio.c)
    set(AUTOSAR_STARTUP_SOURCE "")
endif()

add_library(bsw_mcal STATIC
    ${AUTOSAR_MCU_SOURCE}
    ${AUTOSAR_PORT_SOURCE}
    ${AUTOSAR_GPT_SOURCE}
    ${AUTOSAR_SYSTIMER_SOURCE}
    bsw/mcal/SysTimer/SysTimer_Cfg.c
    ${AUTOSAR_VIRTUALHW_SOURCE}
    bsw/mcal/VirtualHw/VirtualHw_Parse.c
    ${AUTOSAR_ADC_SOURCE}
    ${AUTOSAR_DIO_SOURCE}
    bsw/mcal/Display/Display.c
    ${AUTOSAR_CAN_SOURCE}
    ${AUTOSAR_STARTUP_SOURCE}
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
    bsw/mcal/VirtualHw
    platform/s32k144
    bsw/services/Det # Mcu/Port/Gpt/SysTimer/Can report to Det; header-only dependency, resolved at final link
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
# ECU executables (section 12.2)
# ---------------------------------------------------------------------------
add_executable(sensor_ecu
    ecu/SensorEcu/main.c
    ecu/SensorEcu/config/SensorEcu_Cfg.c
)
target_include_directories(sensor_ecu PRIVATE ecu/SensorEcu/config)
# bsw_services comes first because EcuM lives there and calls Rte_Start,
# which the ECU-specific RTE provides; the BSW cycle declared above makes
# the archives repeat for the calls that go the other way.
target_link_libraries(sensor_ecu PRIVATE
    bsw_services
    rte_sensor
    asw_sensor
    bsw_ecuab
    bsw_com
    bsw_mcal
    platform
)
autosar_apply_warnings(sensor_ecu)

if(AUTOSAR_MCAL_VARIANT STREQUAL "s32k144")
    add_executable(sensor_ecu_s32k144
        ecu/SensorEcu/main_s32k144.c
        ecu/SensorEcu/config/SensorEcu_Cfg.c
    )
    target_include_directories(sensor_ecu_s32k144 PRIVATE ecu/SensorEcu/config)
    target_link_libraries(sensor_ecu_s32k144 PRIVATE
        bsw_services
        rte_sensor
        asw_sensor
        bsw_ecuab
        bsw_com
        bsw_mcal
        platform
    )
    autosar_apply_warnings(sensor_ecu_s32k144)
endif()

add_executable(cluster_ecu
    ecu/ClusterEcu/main.c
    ecu/ClusterEcu/config/ClusterEcu_Cfg.c
)
target_include_directories(cluster_ecu PRIVATE ecu/ClusterEcu/config)
target_link_libraries(cluster_ecu PRIVATE
    bsw_services
    rte_cluster
    asw_cluster
    bsw_ecuab
    bsw_com
    bsw_mcal
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
# Phase 1 unit tests: Os and Det/Dem, built against fake timers instead of
# the real bsw_mcal ones, so scheduler/timeout behavior is deterministic
# regardless of host speed (section 16, Phase 1; section 14.1). The fake's
# .c file is compiled directly into each test executable (not through a
# library) so its symbols always win over the real driver — these tests
# never link bsw_mcal. Since the SysTimer/Os_Counter split, the scheduler
# is driven by the fake SysTimer and Det/Dem still by the fake Gpt.
# ---------------------------------------------------------------------------
add_executable(test_scheduler
    tests/unit/test_scheduler.c
    tests/fixtures/SysTimer_Fake.c
    bsw/mcal/SysTimer/SysTimer_Cfg.c # pure const data, no clock; only the driver is faked
)
target_include_directories(test_scheduler PRIVATE tests/fixtures bsw/mcal/SysTimer)
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
# SysTimer / Os_Counter: the OS's own time source, separated from Gpt
# (section 8.1). test_systimer exercises the REAL driver against the real
# host clock (it is the module that owns clock_gettime for the OS, so a
# double would test nothing); test_os_counter exercises the REAL counter
# against the FAKE SysTimer, in fully simulated time.
# ---------------------------------------------------------------------------
add_executable(test_systimer
    tests/unit/test_systimer.c
    bsw/mcal/SysTimer/SysTimer.c
    bsw/mcal/SysTimer/SysTimer_Cfg.c
)
target_include_directories(test_systimer PRIVATE bsw/mcal/SysTimer)
target_link_libraries(test_systimer PRIVATE bsw_services platform)
autosar_apply_warnings(test_systimer)
add_test(NAME test_systimer COMMAND test_systimer)

add_executable(test_os_counter
    tests/unit/test_os_counter.c
    bsw/os/Os/Os_Counter.c
    tests/fixtures/SysTimer_Fake.c
    bsw/mcal/SysTimer/SysTimer_Cfg.c
)
target_include_directories(test_os_counter PRIVATE tests/fixtures bsw/mcal/SysTimer bsw/os/Os)
target_link_libraries(test_os_counter PRIVATE bsw_services platform)
autosar_apply_warnings(test_os_counter)
add_test(NAME test_os_counter COMMAND test_os_counter)

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

# ---------------------------------------------------------------------------
# Phase 3 unit tests: Can, CanIf (section 16, Phase 3; section 14.1).
#
# test_can exercises the REAL Can.c (real PF_CAN/SOCK_RAW sockets on the
# real "vcan0" interface -- this is the layer that owns the socket, same
# rationale as test_virtual_hw) against a FAKE CanIf
# (tests/fixtures/CanIf_Fake.c), so assertions can inspect exactly what
# Can.c handed upward without depending on CanIf's own routing logic. It
# never links bsw_mcal or bsw_com (would duplicate these same .c files via
# the archive); the needed real sources are compiled directly into the
# test executable, same seam as Phase 1/2. Requires vcan0 to exist and be
# up (tools/setup_vcan.sh; see README.md).
#
# test_canif exercises the REAL CanIf.c against a FAKE Can
# (tests/fixtures/Can_Fake.c) and a FAKE PduR (tests/fixtures/PduR_Fake.c,
# added in Phase 4 once CanIf.c started forwarding to PduR), for full,
# deterministic control over controller state and Can_Write's result --
# exactly the section 16 Phase 3 deliverable ("in-memory or mocked
# transport for unit tests").
# ---------------------------------------------------------------------------
add_executable(test_can
    tests/unit/test_can.c
    bsw/mcal/Can/Can.c
    tests/fixtures/CanIf_Fake.c
)
target_include_directories(test_can PRIVATE
    bsw/mcal/Can
    bsw/communication/CanIf
    tests/fixtures
)
target_link_libraries(test_can PRIVATE bsw_services platform)
autosar_apply_warnings(test_can)
add_test(NAME test_can COMMAND test_can)

add_executable(test_canif
    tests/unit/test_canif.c
    bsw/communication/CanIf/CanIf.c
    tests/fixtures/Can_Fake.c
    tests/fixtures/PduR_Fake.c
)
target_include_directories(test_canif PRIVATE
    bsw/communication/CanIf
    bsw/communication/PduR
    bsw/mcal/Can
    tests/fixtures
)
target_link_libraries(test_canif PRIVATE bsw_services platform)
autosar_apply_warnings(test_canif)
add_test(NAME test_canif COMMAND test_canif)

# ---------------------------------------------------------------------------
# Phase 4 unit tests: ComProtection, Com, PduR (section 16, Phase 4;
# section 14.1).
#
# test_com_protection exercises the REAL, pure, stateless ComProtection.c
# directly -- no Det/Gpt/Com dependency at all, so it needs no fixtures
# and does not link bsw_services.
#
# test_com_pack exercises the REAL Com.c and ComProtection.c against a
# FAKE PduR (tests/fixtures/PduR_Fake.c, to capture exactly what bytes
# were requested to send) and a FAKE Gpt (tests/fixtures/Gpt_Fake.c, for
# deterministic cyclic-Tx timing and Rx deadline supervision) -- exactly
# the Phase 4 exit criteria ("raw CAN injection reaches a mock RTE
# callback; COM Tx produces exact expected bytes").
#
# test_pdur exercises the REAL PduR.c against a FAKE CanIf (its
# CanIf_Transmit role from tests/fixtures/CanIf_Fake.c) and a FAKE Com
# (tests/fixtures/Com_Fake.c), so PduR's own id-only routing logic is
# tested deterministically without a real CAN transport or signal
# configuration.
# ---------------------------------------------------------------------------
add_executable(test_com_protection
    tests/unit/test_com_protection.c
    bsw/communication/Com/ComProtection.c
)
target_include_directories(test_com_protection PRIVATE bsw/communication/Com)
target_link_libraries(test_com_protection PRIVATE platform)
autosar_apply_warnings(test_com_protection)
add_test(NAME test_com_protection COMMAND test_com_protection)

add_executable(test_com_pack
    tests/unit/test_com_pack.c
    bsw/communication/Com/Com.c
    bsw/communication/Com/ComProtection.c
    tests/fixtures/PduR_Fake.c
    tests/fixtures/Gpt_Fake.c
)
target_include_directories(test_com_pack PRIVATE
    bsw/communication/Com
    bsw/communication/PduR
    bsw/mcal/Gpt
    tests/fixtures
)
target_link_libraries(test_com_pack PRIVATE bsw_services platform)
autosar_apply_warnings(test_com_pack)
add_test(NAME test_com_pack COMMAND test_com_pack)

add_executable(test_pdur
    tests/unit/test_pdur.c
    bsw/communication/PduR/PduR.c
    tests/fixtures/CanIf_Fake.c
    tests/fixtures/Com_Fake.c
)
target_include_directories(test_pdur PRIVATE
    bsw/communication/PduR
    bsw/communication/CanIf
    bsw/communication/Com
    bsw/mcal/Can
    bsw/mcal/Gpt
    tests/fixtures
)
target_link_libraries(test_pdur PRIVATE bsw_services platform)
autosar_apply_warnings(test_pdur)
add_test(NAME test_pdur COMMAND test_pdur)

# ---------------------------------------------------------------------------
# Phase 5 unit tests: IoHwAb, both RTEs, and the cluster SW-C
# (section 16, Phase 5; section 14.1).
#
# test_iohwab runs the REAL IoHwAb against fake Adc/Dio/Display/Gpt and the
# REAL Dem, so the DEM consequences of REQ-IOHWAB-004 are observable.
# The RTE tests run the REAL RTE and REAL SW-C against a fake Com and a
# fake IoHwAb. test_cluster_swc runs the REAL SW-C against a fake RTE,
# which is what makes every branch of the section 4.4 state machine
# reachable directly.
# ---------------------------------------------------------------------------
add_executable(test_iohwab
    tests/unit/test_iohwab.c
    bsw/ecu_abstraction/IoHwAb/IoHwAb.c
    bsw/ecu_abstraction/IoHwAb/IoHwAb_Cfg.c
    tests/fixtures/Adc_Fake.c
    tests/fixtures/Dio_Fake.c
    tests/fixtures/Display_Fake.c
    tests/fixtures/Gpt_Fake.c
)
target_include_directories(test_iohwab PRIVATE
    tests/fixtures
    bsw/ecu_abstraction/IoHwAb
    bsw/mcal/Adc
    bsw/mcal/Dio
    bsw/mcal/Display
    bsw/mcal/Gpt
    rte/common
)
target_link_libraries(test_iohwab PRIVATE bsw_services platform)
autosar_apply_warnings(test_iohwab)
add_test(NAME test_iohwab COMMAND test_iohwab)

add_executable(test_rte_sensor
    tests/unit/test_rte_sensor.c
    rte/generated/SensorEcu/Rte_SensorEcu.c
    asw/VehicleStateSwc/VehicleStateSwc.c
    tests/fixtures/Com_Fake.c
    tests/fixtures/IoHwAb_Fake.c
)
target_include_directories(test_rte_sensor PRIVATE
    tests/fixtures
    rte/common
    rte/generated/SensorEcu
    asw/VehicleStateSwc
    bsw/ecu_abstraction/IoHwAb
    bsw/communication/Com
    bsw/mcal/Gpt
)
target_link_libraries(test_rte_sensor PRIVATE bsw_services platform)
autosar_apply_warnings(test_rte_sensor)
add_test(NAME test_rte_sensor COMMAND test_rte_sensor)

add_executable(test_rte_cluster
    tests/unit/test_rte_cluster.c
    rte/generated/ClusterEcu/Rte_ClusterEcu.c
    asw/ClusterControlSwc/ClusterControlSwc.c
    tests/fixtures/Com_Fake.c
    tests/fixtures/IoHwAb_Fake.c
)
target_include_directories(test_rte_cluster PRIVATE
    tests/fixtures
    rte/common
    rte/generated/ClusterEcu
    asw/ClusterControlSwc
    bsw/ecu_abstraction/IoHwAb
    bsw/communication/Com
    bsw/mcal/Gpt
)
target_link_libraries(test_rte_cluster PRIVATE bsw_services platform)
autosar_apply_warnings(test_rte_cluster)
add_test(NAME test_rte_cluster COMMAND test_rte_cluster)

add_executable(test_cluster_swc
    tests/unit/test_cluster_swc.c
    asw/ClusterControlSwc/ClusterControlSwc.c
    tests/fixtures/Rte_ClusterFake.c
)
target_include_directories(test_cluster_swc PRIVATE
    tests/fixtures
    rte/common
    rte/generated/ClusterEcu
    asw/ClusterControlSwc
)
target_link_libraries(test_cluster_swc PRIVATE platform)
autosar_apply_warnings(test_cluster_swc)
add_test(NAME test_cluster_swc COMMAND test_cluster_swc)

# ---------------------------------------------------------------------------
# Phase 6: configuration validation (section 14.1 "test_config",
# section 11.3). Runs the REAL CanIf/PduR/Com/Os validation against fake
# drivers — validation IS the behavior under test, so a fake validator
# would prove nothing. The tables mirror the shipping SensorEcu config.
# ---------------------------------------------------------------------------
add_executable(test_config
    tests/unit/test_config.c
    bsw/communication/CanIf/CanIf.c
    bsw/communication/PduR/PduR.c
    bsw/communication/Com/Com.c
    bsw/communication/Com/ComProtection.c
    bsw/os/Os/Os_Counter.c
    tests/fixtures/Can_Fake.c
    tests/fixtures/Gpt_Fake.c
    tests/fixtures/SysTimer_Fake.c
    bsw/mcal/SysTimer/SysTimer_Cfg.c
)
target_include_directories(test_config PRIVATE
    tests/fixtures
    bsw/communication/CanIf
    bsw/communication/PduR
    bsw/communication/Com
    bsw/mcal/Can
    bsw/mcal/Gpt
    bsw/mcal/SysTimer
    bsw/os/Os
)
target_link_libraries(test_config PRIVATE bsw_services platform)
autosar_apply_warnings(test_config)
add_test(NAME test_config COMMAND test_config)

add_executable(test_bswm
    tests/unit/test_bswm.c
)
target_include_directories(test_bswm PRIVATE
    bsw/services/BswM
    bsw/services/Det
)
target_link_libraries(test_bswm PRIVATE bsw_services platform)
autosar_apply_warnings(test_bswm)
add_test(NAME test_bswm COMMAND test_bswm)

add_executable(test_cansm
    tests/unit/test_cansm.c
    bsw/communication/CanSM/CanSM.c
)
target_include_directories(test_cansm PRIVATE
    bsw/communication/CanSM
    bsw/communication/CanIf
    bsw/services/Det
)
target_link_libraries(test_cansm PRIVATE bsw_services platform)
autosar_apply_warnings(test_cansm)
add_test(NAME test_cansm COMMAND test_cansm)

add_executable(test_comm
    tests/unit/test_comm.c
    bsw/services/ComM/ComM.c
)
target_include_directories(test_comm PRIVATE
    bsw/services/ComM
    bsw/communication/CanSM
    bsw/communication/Com
    bsw/services/Det
)
target_link_libraries(test_comm PRIVATE bsw_services platform)
autosar_apply_warnings(test_comm)
add_test(NAME test_comm COMMAND test_comm)

add_executable(test_os_lifecycle
    tests/unit/test_os_lifecycle.c
    tests/fixtures/SysTimer_Fake.c
    bsw/mcal/SysTimer/SysTimer_Cfg.c
)
target_include_directories(test_os_lifecycle PRIVATE
    bsw/os/Os
    bsw/services/EcuM
    bsw/services/BswM
    bsw/services/Det
    bsw/mcal/SysTimer
    tests/fixtures
)
target_link_libraries(test_os_lifecycle PRIVATE bsw_services platform)
target_compile_definitions(test_os_lifecycle PRIVATE UNIT_TEST=1)
autosar_apply_warnings(test_os_lifecycle)
add_test(NAME test_os_lifecycle COMMAND test_os_lifecycle)

# ---------------------------------------------------------------------------
# REQ-ARCH-012 proof: the REAL Adc.c/Dio.c/CanIf.c/VirtualHw_Parse.c,
# completely unmodified, linked against the STUB Can_Stub.c/
# VirtualHw_Stub.c instead of their real counterparts -- exactly the swap
# -DAUTOSAR_MCAL_VARIANT=stub makes for the full ECU executables. This is
# independent of that CMake option (it always uses the stub sources
# directly) so the claim is checked on every normal `ctest` run, not only
# when someone remembers to reconfigure with the option set.
# ---------------------------------------------------------------------------
add_executable(test_stub_mcal
    tests/unit/test_stub_mcal.c
    bsw/mcal/Can/Can_Stub.c
    bsw/mcal/VirtualHw/VirtualHw_Stub.c
    bsw/mcal/VirtualHw/VirtualHw_Parse.c
    bsw/mcal/Adc/Adc.c
    bsw/mcal/Dio/Dio.c
    bsw/communication/CanIf/CanIf.c
    bsw/services/Det/Det.c # linked directly, NOT via bsw_services: that target's PUBLIC
                            # link to the REAL bsw_mcal would pull in the real Can.c/
                            # VirtualHw.c/Adc.c/Dio.c too, colliding with the stub/direct
                            # sources above (multiple definition at link time).
    tests/fixtures/Gpt_Fake.c
    tests/fixtures/PduR_Fake.c # CanIf.c forwards to PduR; the PduR routing decision
                                # itself is not what this file tests, so a fake suffices
)
target_include_directories(test_stub_mcal PRIVATE
    tests/fixtures
    bsw/services/Det
    bsw/mcal/Can
    bsw/mcal/VirtualHw
    bsw/mcal/Adc
    bsw/mcal/Dio
    bsw/mcal/Gpt
    bsw/communication/CanIf
    bsw/communication/PduR
)
target_link_libraries(test_stub_mcal PRIVATE platform)
autosar_apply_warnings(test_stub_mcal)
add_test(NAME test_stub_mcal COMMAND test_stub_mcal)

# ---------------------------------------------------------------------------
# Section 14.2 integration tests: IT-01 through IT-07, all seven now
# implemented (IT-01/02/05/06/07 were the Phase 6 exit criterion;
# IT-03/04 are Phase 8's). These start the REAL sensor_ecu/cluster_ecu
# binaries as subprocesses and drive them through real AF_UNIX virtual
# hardware links, real vcan0, and (IT-03) hand-built raw CAN frames --
# like test_can, they need vcan0 already up (tools/setup_vcan.sh) and are
# excluded from the default run via the "integration" label so `ctest`
# alone stays fast; run them explicitly with `ctest -L integration`. Each
# takes several seconds (real process start/stop, real timing windows),
# unlike the millisecond unit tests above.
# ---------------------------------------------------------------------------
foreach(it_name IN ITEMS
    test_it01_normal_e2e
    test_it02_timeout_recovery
    test_it03_integrity_rejection
    test_it04_layered_evidence
    test_it05_virtual_hw_input
    test_it06_sensor_disconnect
    test_it07_display_absent
)
    # python3, not a discovered interpreter: the whole project's own run/
    # test commands (section 15.2-15.3, tools/run_demo.sh) already assume
    # a `python3` on PATH, so this matches rather than adds a second
    # convention. These tests also assume sensor_ecu/cluster_ecu were
    # already built by the same `cmake --build` that precedes `ctest` in
    # every documented workflow (section 15.2) -- ctest itself has no
    # portable "build this target first" dependency to express here.
    add_test(NAME ${it_name}
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/integration/${it_name}.py
    )
    set_tests_properties(${it_name} PROPERTIES
        LABELS integration
        TIMEOUT 60
    )
endforeach()
