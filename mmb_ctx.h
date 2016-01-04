#ifndef MMB_CTX_H_
#define MMB_CTX_H_

#include <bluetooth/bluetooth.h>

typedef struct mmb_ctx_s    mmb_ctx, * MMB_CTX;

enum mmb_status_e {
    MMB_STATUS_STOPPED      = -1,
    MMB_STATUS_INITIAL      = 0,
    MMB_STATUS_CONNECTING   = 1,
    MMB_STATUS_CONNECTED    = 2,
};

struct mmb_ctx_s {
    int                         status;
    struct evhr_ctx_s *         evhr;
    struct mmb_event_ctx_s *    eventer;
    struct wpool_ctx_s *        wpool;
    struct eble_adapter_ctx_s * adapter;
    struct qlist_ctx_s *        devices;
};

int mmb_service_init(bdaddr_t * ble_adapter_addr);
int mmb_service_start();

#endif
