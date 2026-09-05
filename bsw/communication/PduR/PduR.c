/**
 * @file PduR.c
 * @brief Static routing implementation. No payload inspection anywhere in
 * this file (`REQ-ARCH-003`).
 */
#include "PduR.h"

#include <stddef.h>
#include <string.h>

#include "CanIf.h"
#include "Com.h"
#include "Det.h"
#include "Det_ModuleIds.h"

#define PDUR_API_ID_INIT ((Det_ApiIdType)1U)
#define PDUR_API_ID_COM_TRANSMIT ((Det_ApiIdType)2U)
#define PDUR_API_ID_CANIF_RX_INDICATION ((Det_ApiIdType)3U)
#define PDUR_API_ID_CANIF_TX_CONFIRMATION ((Det_ApiIdType)4U)

/* This project has exactly two routes per ECU (section 7.5): one Tx, one
 * Rx. Sized a little above that, matching the small-fixed-table
 * convention used throughout this project. */
#define PDUR_MAX_ROUTES ((uint8)4U)

typedef struct {
    boolean initialized;
    PduR_RouteType routes[PDUR_MAX_ROUTES];
    uint8 numRoutes;
} PduR_StateType;

static PduR_StateType g_state;

static const PduR_RouteType *FindRouteBySource(PduR_ModuleType module, PduIdType pduId) {
    for (uint8 i = 0U; i < g_state.numRoutes; i++) {
        if ((g_state.routes[i].sourceModule == module) &&
            (g_state.routes[i].sourcePduId == pduId)) {
            return &g_state.routes[i];
        }
    }
    return NULL;
}

static const PduR_RouteType *FindRouteByDest(PduR_ModuleType module, PduIdType pduId) {
    for (uint8 i = 0U; i < g_state.numRoutes; i++) {
        if ((g_state.routes[i].destModule == module) && (g_state.routes[i].destPduId == pduId)) {
            return &g_state.routes[i];
        }
    }
    return NULL;
}

void PduR_Init(const PduR_ConfigType *config) {
    /* Set unconditionally before any validation, so a re-Init that fails
     * partway through can never leave a PREVIOUS successful init's
     * routes silently active. */
    g_state.initialized = FALSE;

    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_INIT, DET_ERR_NULL_POINTER);
        return;
    }
    if ((config->routes == NULL) || (config->numRoutes == 0U) ||
        (config->numRoutes > PDUR_MAX_ROUTES)) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_INIT, DET_ERR_INVALID_CONFIG);
        return;
    }

    /* REQ-CFG-001/004: reviewable const tables, validated at init,
     * all-or-nothing (same convention as CanIf_Init) -- reject a
     * duplicate (sourceModule, sourcePduId) pair, which would make a
     * transmit/indication request ambiguous. */
    for (uint8 i = 0U; i < config->numRoutes; i++) {
        for (uint8 j = 0U; j < i; j++) {
            if ((config->routes[j].sourceModule == config->routes[i].sourceModule) &&
                (config->routes[j].sourcePduId == config->routes[i].sourcePduId)) {
                (void)Det_ReportError(DET_MODULE_ID_PDUR, i, PDUR_API_ID_INIT,
                                      DET_ERR_INVALID_CONFIG);
                return;
            }
        }
    }

    (void)memcpy(g_state.routes, config->routes,
                 (size_t)config->numRoutes * sizeof(g_state.routes[0]));
    g_state.numRoutes = config->numRoutes;
    g_state.initialized = TRUE;
}

void PduR_DeInit(void) {
    (void)memset(&g_state, 0, sizeof(g_state));
}

Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *info) {
    if (!g_state.initialized) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_COM_TRANSMIT, DET_ERR_UNINIT);
        return E_NOT_OK;
    }
    const PduR_RouteType *route = FindRouteBySource(PDUR_MODULE_COM, id);
    if ((route == NULL) || (route->destModule != PDUR_MODULE_CANIF)) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_COM_TRANSMIT,
                              DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    return CanIf_Transmit(route->destPduId, info);
}

void PduR_CanIfRxIndication(PduIdType id, const PduInfoType *info) {
    if (!g_state.initialized) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_CANIF_RX_INDICATION,
                              DET_ERR_UNINIT);
        return;
    }
    const PduR_RouteType *route = FindRouteBySource(PDUR_MODULE_CANIF, id);
    if ((route == NULL) || (route->destModule != PDUR_MODULE_COM)) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_CANIF_RX_INDICATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    Com_RxIndication(route->destPduId, info);
}

void PduR_CanIfTxConfirmation(PduIdType id) {
    if (!g_state.initialized) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_CANIF_TX_CONFIRMATION,
                              DET_ERR_UNINIT);
        return;
    }
    /* Reverse lookup: a Tx confirmation flows back along the same route
     * a PduR_ComTransmit request took, so it is found by matching the
     * CanIf-side id against the route's DEST, not its source. */
    const PduR_RouteType *route = FindRouteByDest(PDUR_MODULE_CANIF, id);
    if ((route == NULL) || (route->sourceModule != PDUR_MODULE_COM)) {
        (void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, PDUR_API_ID_CANIF_TX_CONFIRMATION,
                              DET_ERR_INVALID_PARAM);
        return;
    }
    Com_TxConfirmation(route->sourcePduId);
}
