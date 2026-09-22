#include "Mcu.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define MCU_API_ID_INIT ((Det_ApiIdType)1U)
#define MCU_API_ID_INIT_CLOCK ((Det_ApiIdType)2U)

/* Vai trò: mô phỏng bước khởi tạo lõi MCU đầu tiên trong chuỗi boot —
 * trên chip thật sẽ cấu hình reset source/RAM/clock tree; ở Ubuntu không
 * có phần cứng đó nên chỉ validate con trỏ config cho đúng "hợp đồng"
 * gọi hàm (REQ-CFG-001). */
void Mcu_Init(const Mcu_ConfigType *config) {
    /* Deviation: a real Mcu_Init configures reset sources, RAM sections,
     * and the clock tree from `config`. Ubuntu has no such hardware; this
     * only validates the pointer for symmetry with a real target's
     * initialization contract (REQ-CFG-001). */
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_MCU, 0U, MCU_API_ID_INIT, DET_ERR_NULL_POINTER);
    }
}

/* Vai trò: mô phỏng bước bật clock tree — chỉ chấp nhận đúng 1 giá trị
 * mặc định (không có clock thật để cấu hình). */
Std_ReturnType Mcu_InitClock(Mcu_ClockType clock_setting) {
    if (clock_setting != MCU_CLOCK_DEFAULT) {
        (void)Det_ReportError(DET_MODULE_ID_MCU, 0U, MCU_API_ID_INIT_CLOCK, DET_ERR_INVALID_PARAM);
        return E_NOT_OK;
    }
    return E_OK;
}
