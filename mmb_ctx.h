#ifndef MMB_CTX_H_
#define MMB_CTX_H_

#define MMB_BUFFER_SIZE             512

enum mmb_status_e {
    MMB_STATUS_STOPPED      = -1,
    MMB_STATUS_INITIAL      = 0,
    MMB_STATUS_CONNECTING   = 1,
    MMB_STATUS_CONNECTED    = 2,
};

#define MMB_EV_UNKNOW               0x0000
#define MMB_EV_SCAN_REQ             0x0201
#define MMB_EV_SCAN_RESP            0x0202

typedef struct mmb_ctx_s {
    int                         status;
    struct evhr_ctx_s *         evhr;
    struct mmb_adapter_s *      adapter;
    struct qlist_ctx_s *        devices;
} MMB_CTX;

#endif
