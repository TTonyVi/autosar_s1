/**
 * @file Det.h
 * @brief Development Error Tracer (simplified).
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 10.1. Records development
 * / programming errors (uninitialized module use, null pointers, invalid
 * IDs, ...) reported by other modules. It is a passive recorder: it never
 * allocates memory, never terminates the process, and is not an
 * alternative data path for application signals.
 */
#ifndef DET_H
#define DET_H

#include "Det_ModuleIds.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint8 Det_InstanceIdType;
typedef uint8 Det_ApiIdType;
typedef uint8 Det_ErrorIdType;

/* Project-local error ID registry (spec section 10.1's condition list).
 * Extend as later phases need more specific conditions (invalid PDU ID,
 * wrong DLC, malformed datagram, ADC group with no result, ...); never
 * reuse a retired ID. */
#define DET_ERR_UNINIT ((Det_ErrorIdType)1U)
#define DET_ERR_NULL_POINTER ((Det_ErrorIdType)2U)
#define DET_ERR_INVALID_PARAM ((Det_ErrorIdType)3U)
#define DET_ERR_INVALID_CONFIG ((Det_ErrorIdType)4U)
#define DET_ERR_SCHEDULER_OVERRUN ((Det_ErrorIdType)5U)

/** Ring buffer capacity per spec section 10.1 ("latest 64 records"). */
#define DET_MAX_RECORDS ((uint32)64U)

typedef struct {
    Det_ModuleIdType moduleId;
    Det_InstanceIdType instanceId;
    Det_ApiIdType apiId;
    Det_ErrorIdType errorId;
} Det_RecordType;

void Det_Init(void);
void Det_DeInit(void);

Std_ReturnType Det_ReportError(Det_ModuleIdType module_id, Det_InstanceIdType instance_id,
                               Det_ApiIdType api_id, Det_ErrorIdType error_id);

/* Read-only inspection surface for tests and, later, diagnostic log
 * dumps. Not an alternate data path (spec section 12.1) — nothing but
 * tests and logging code should call these. */
uint32 Det_GetTotalErrorCount(void);
uint32 Det_GetModuleErrorCount(Det_ModuleIdType module_id);
uint32 Det_GetRecordCount(void);
Std_ReturnType Det_GetRecord(uint32 indexFromOldest, Det_RecordType *out);

#endif /* DET_H */
