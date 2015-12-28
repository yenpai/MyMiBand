#ifndef MMB_CTX_H_
#define MMB_CTX_H_

#define MMB_BUFFER_SIZE             512

enum mmb_status_e {
    MMB_STATUS_STOPPED      = -1,
    MMB_STATUS_INITIAL      = 0,
    MMB_STATUS_CONNECTING   = 1,
    MMB_STATUS_CONNECTED    = 2,
};

typedef struct mmb_ctx_s {
    int                         status;
    struct evhr_ctx_s *         evhr;
    struct mmb_event_ctx_s *    eventer;
    struct mmb_adapter_s *      adapter;
    struct qlist_ctx_s *        devices;
} MMB_CTX;

#endif
