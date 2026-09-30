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
static uint32 g_comTransmitForwardCount;
static uint32 g_canIfRxIndicationForwardCount;

/* Vai trò: tìm route theo chiều "nguồn gửi" — dùng khi Com hoặc CanIf báo
 * "tôi có 1 PDU với id X muốn gửi/đã nhận", để biết nó thuộc route nào. */
static const PduR_RouteType *FindRouteBySource(PduR_ModuleType module, PduIdType pduId) {
    for (uint8 i = 0U; i < g_state.numRoutes; i++) {
        if ((g_state.routes[i].sourceModule == module) &&
            (g_state.routes[i].sourcePduId == pduId)) {
            return &g_state.routes[i];
        }
    }
    return NULL;
}

/* Vai trò: tìm route theo chiều "đích đến" — ngược lại với hàm trên,
 * dùng khi cần tra route theo id ở đầu NHẬN (vd: CanIf xác nhận đã gửi
 * xong 1 PDU, PduR cần lần ngược lại xem PDU đó ban đầu đến từ Com nào). */
static const PduR_RouteType *FindRouteByDest(PduR_ModuleType module, PduIdType pduId) {
    for (uint8 i = 0U; i < g_state.numRoutes; i++) {
        if ((g_state.routes[i].destModule == module) && (g_state.routes[i].destPduId == pduId)) {
            return &g_state.routes[i];
        }
    }
    return NULL;
}

/* Vai trò: khởi tạo PduR — nạp bảng route tĩnh (cấu hình sẵn, không đổi
 * lúc chạy) từ config vào state nội bộ, kiểm tra hợp lệ (không NULL,
 * không vượt số route tối đa, không trùng route nguồn) trước khi bật cờ
 * "đã init". Nếu bất kỳ bước validate nào fail thì báo lỗi qua Det và
 * dừng lại — không có route nào được kích hoạt (all-or-nothing). */
void PduR_Init(const PduR_ConfigType *config) {
    /* Set unconditionally before any validation, so a re-Init that fails
     * partway through can never leave a PREVIOUS successful init's
     * routes silently active. */
    g_state.initialized = FALSE;
    g_comTransmitForwardCount = 0U;
    g_canIfRxIndicationForwardCount = 0U;

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

/* Vai trò: dọn sạch toàn bộ state của PduR (xoá bảng route, reset counter,
 * tắt cờ "đã init") — dùng khi ECU shutdown hoặc cần re-init lại từ đầu. */
void PduR_DeInit(void) {
    (void)memset(&g_state, 0, sizeof(g_state));
    g_comTransmitForwardCount = 0U;
    g_canIfRxIndicationForwardCount = 0U;
}

/* Vai trò: nhận yêu cầu gửi PDU từ Com (id = PduId phía Com), tra route
 * xem PDU này phải đi tới CanIf với id nào, rồi forward nguyên xi payload
 * (`info`) xuống CanIf_Transmit() — không đọc/sửa nội dung payload
 * (REQ-ARCH-003), chỉ đổi id định tuyến. */
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
    g_comTransmitForwardCount++;
    return CanIf_Transmit(route->destPduId, info);
}

/* Vai trò: chiều ngược lại của PduR_ComTransmit — CanIf báo "vừa nhận
 * được 1 frame trên bus với PduId = id", PduR tra route rồi forward
 * payload lên Com_RxIndication() để Com giải mã/kiểm tra CRC/counter. */
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
    g_canIfRxIndicationForwardCount++;
    Com_RxIndication(route->destPduId, info);
}

/* Vai trò: CanIf báo "đã gửi xong lên bus 1 frame với PduId = id" — PduR
 * cần lần ngược route (tra theo dest thay vì source) để biết PDU đó ban
 * đầu là của Com nào, rồi báo lại cho Com_TxConfirmation() biết là gửi
 * thành công (Com dùng cái này để, ví dụ, tăng alive counter lần kế). */
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

/* Vai trò: trả về số lần PduR_ComTransmit đã forward thành công xuống
 * CanIf — chỉ dùng để test/quan sát (IT-04, kiểm chứng dữ liệu thật sự
 * đi qua từng tầng), không phải logic nghiệp vụ. */
uint32 PduR_GetComTransmitForwardCount(void) {
    return g_comTransmitForwardCount;
}

/* Vai trò: tương tự trên nhưng đếm chiều nhận — số lần
 * PduR_CanIfRxIndication đã forward lên Com. */
uint32 PduR_GetCanIfRxIndicationForwardCount(void) {
    return g_canIfRxIndicationForwardCount;
}
