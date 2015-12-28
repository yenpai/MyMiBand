#ifndef MMB_ADAPTER_H_
#define MMB_ADAPTER_H_

#include <pthread.h>
#include <bluetooth/bluetooth.h>

#include "evhr.h"
#include "mmb_ble.h"
#include "mmb_event.h"

typedef void (*MMB_ADAPTER_SCAN_CB) (void *pdata, struct qlist_ctx_s * results);

typedef struct mmb_adapter_s {
    int                                 status;
    int                                 dev;
    bdaddr_t                            addr;
    pthread_t                           thread_scan_pid;
    struct evhr_event_s *               ev_scan_timer;
    struct mmb_event_ctx_s *            scan_notify_eventer;
    struct qlist_ctx_s *                scan_results;
} MMB_ADAPTER;

int mmb_adapter_init(MMB_ADAPTER * this, bdaddr_t * addr);
int mmb_adapter_connect(MMB_ADAPTER * this);
int mmb_adapter_disconnect(MMB_ADAPTER * this);

int mmb_adapter_scan_start(MMB_ADAPTER * this, EVHR_CTX * evhr, struct mmb_event_ctx_s * eventer);
int mmb_adapter_scan_stop(MMB_ADAPTER * this);

#endif /* ifndef MMB_ADAPTER_H_ */
