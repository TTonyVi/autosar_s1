/**
 * @file Std_Types.h
 * @brief Project-local, AUTOSAR-styled standard return type.
 *
 * Deviation from real AUTOSAR: the official Std_Types.h also carries
 * Std_VersionInfoType and module vendor/module ID conventions tied to an
 * official module ID registry. This project uses only project-local module
 * IDs (see Det.h in a later phase) and does not claim registry compliance.
 */
#ifndef STD_TYPES_H
#define STD_TYPES_H

#include "Platform_Types.h"

typedef uint8 Std_ReturnType;

#define E_OK ((Std_ReturnType)0x00U)
#define E_NOT_OK ((Std_ReturnType)0x01U)

/** Project-local extended standard levels used by MCAL channel APIs. */
typedef uint8 Std_LevelType;
#define STD_LOW ((Std_LevelType)0x00U)
#define STD_HIGH ((Std_LevelType)0x01U)

#endif /* STD_TYPES_H */
