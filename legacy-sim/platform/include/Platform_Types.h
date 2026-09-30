/**
 * @file Platform_Types.h
 * @brief Project-local, AUTOSAR-styled fixed-width base types.
 *
 * Deviation from real AUTOSAR: the official Platform_Types.h is generated
 * per CPU/compiler combination and defines CPU_TYPE, endianness, and
 * register width macros. This project targets exactly one platform
 * (Ubuntu x86_64), so those selectors are omitted; only the fixed-width
 * typedefs and boolean literals that the rest of the specification
 * actually depends on are provided here.
 *
 * This header SHALL NOT be redefined by any module (see
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 7.3).
 */
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t sint8;
typedef int16_t sint16;
typedef int32_t sint32;
typedef int64_t sint64;

typedef float float32;
typedef double float64;

typedef uint8_t boolean;

#ifndef TRUE
#define TRUE 1u
#endif

#ifndef FALSE
#define FALSE 0u
#endif

#endif /* PLATFORM_TYPES_H */
