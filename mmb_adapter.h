#ifndef MMB_ADAPTER_H_
#define MMB_ADAPTER_H_

#include <pthread.h>
#include <bluetooth/bluetooth.h>

#include "evhr.h"
#include "mmb_ble.h"

typedef void (*MMB_ADAPTER_SCAN_CB) (void *pdata, struct mmb_adapter_scan_result_s * results, int size);

typedef struct mmb_adapter_s {
    int                                 status;
    int                                 dev;
    bdaddr_t                            addr;
    pthread_t                           thread_scan_pid;
    struct evhr_event_s *               ev_scan_notify;
    struct evhr_event_s *               ev_scan_timer;
    MMB_ADAPTER_SCAN_CB                 cb_scan;
    void *                              cb_scan_pdata;
    struct mmb_adapter_scan_result_s *  scan_resp;
    int                                 scan_resp_counts;
} MMB_ADAPTER;

int mmb_adapter_init(MMB_ADAPTER * this, bdaddr_t * addr);
int mmb_adapter_connect(MMB_ADAPTER * this, struct evhr_ctx_s * evhr);
int mmb_adapter_disconnect(MMB_ADAPTER * this);

int mmb_adapter_scan_start(MMB_ADAPTER * this, EVHR_CTX * evhr, MMB_ADAPTER_SCAN_CB cb, void *pdata);
int mmb_adapter_scan_stop(MMB_ADAPTER * this);

#endif /* ifndef MMB_ADAPTER_H_ */
