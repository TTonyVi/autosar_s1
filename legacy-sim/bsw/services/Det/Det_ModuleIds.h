/**
 * @file Det_ModuleIds.h
 * @brief Project-local Det module ID registry.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 10.1: "Numeric module IDs
 * are local to this project and SHALL not be advertised as official
 * AUTOSAR IDs." Extend this list as later phases add modules that report
 * to Det; never reuse or renumber a retired ID, since old test
 * expectations and log output would silently point at the wrong module.
 */
#ifndef DET_MODULE_IDS_H
#define DET_MODULE_IDS_H

#include "Platform_Types.h"

typedef uint16 Det_ModuleIdType;

#define DET_MODULE_ID_MCU ((Det_ModuleIdType)1U)
#define DET_MODULE_ID_PORT ((Det_ModuleIdType)2U)
#define DET_MODULE_ID_GPT ((Det_ModuleIdType)3U)
#define DET_MODULE_ID_DEM ((Det_ModuleIdType)4U)
#define DET_MODULE_ID_OS ((Det_ModuleIdType)5U)
#define DET_MODULE_ID_VIRTUALHW ((Det_ModuleIdType)6U)
#define DET_MODULE_ID_ADC ((Det_ModuleIdType)7U)
#define DET_MODULE_ID_DIO ((Det_ModuleIdType)8U)
#define DET_MODULE_ID_DISPLAY ((Det_ModuleIdType)9U)
#define DET_MODULE_ID_CAN ((Det_ModuleIdType)10U)
#define DET_MODULE_ID_CANIF ((Det_ModuleIdType)11U)
#define DET_MODULE_ID_PDUR ((Det_ModuleIdType)12U)
#define DET_MODULE_ID_COM ((Det_ModuleIdType)13U)
#define DET_MODULE_ID_SYSTIMER ((Det_ModuleIdType)14U)
#define DET_MODULE_ID_IOHWAB ((Det_ModuleIdType)15U)
#define DET_MODULE_ID_RTE ((Det_ModuleIdType)16U)
#define DET_MODULE_ID_ECUM ((Det_ModuleIdType)17U)
#define DET_MODULE_ID_BSWM ((Det_ModuleIdType)18U)
#define DET_MODULE_ID_CANSM ((Det_ModuleIdType)19U)
#define DET_MODULE_ID_COMM ((Det_ModuleIdType)20U)
/* 21+ reserved for modules later phases may add. */

/** Bound on the per-module counter table in Det.c; keep >= the highest ID above. */
#define DET_MAX_MODULE_ID ((Det_ModuleIdType)25U)

#endif /* DET_MODULE_IDS_H */
