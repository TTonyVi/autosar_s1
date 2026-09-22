#include "Port.h"

#include <stddef.h>

#include "Det.h"
#include "Det_ModuleIds.h"

#define PORT_API_ID_INIT ((Det_ApiIdType)1U)

/* Vai trò: mô phỏng bước cấu hình chân GPIO — trên chip thật sẽ set
 * direction/pull/alternate-function; Ubuntu không có pin thật nên chỉ
 * validate con trỏ config. */
void Port_Init(const Port_ConfigType *config) {
    /* Deviation: a real Port_Init configures GPIO direction/pull/alternate
     * function registers from `config`. Ubuntu has no pins to configure;
     * this only validates the pointer for symmetry with a real target
     * (REQ-CFG-001). */
    if (config == NULL) {
        (void)Det_ReportError(DET_MODULE_ID_PORT, 0U, PORT_API_ID_INIT, DET_ERR_NULL_POINTER);
    }
}
