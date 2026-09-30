# cmake/s32k144_toolchain.cmake
# Cross-compilation toolchain file for NXP S32K144 (ARM Cortex-M4F)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

# Cross compiler prefixes
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE_UTIL    ${TOOLCHAIN_PREFIX}size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Target Architecture Flags for ARM Cortex-M4F with hardware floating point
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS}")

# Linker flags: Linker script, nano.specs for small embedded footprint, GC sections
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS} -T${CMAKE_SOURCE_DIR}/platform/s32k144/S32K144_flash.ld -specs=nano.specs -specs=nosys.specs -Wl,--gc-sections")
