#ifndef MMB_ADAPTER_H_
#define MMB_ADAPTER_H_

#include <pthread.h>
#include <bluetooth/bluetooth.h>

#include "evhr.h"

#define MMB_ADAPTER_SCAN_TIMEOUT     30000

struct mmb_adapter_scan_result_s {
    bdaddr_t addr;
    uint8_t  rssi;
    char     name[32];
    struct mmb_adapter_scan_result_s * next;
};

typedef void (*MMB_ADAPTER_SCAN_CB) (struct mmb_adapter_scan_result_s *, size_t size);

typedef struct mmb_adapter_s {
    int                                 status;
    int                                 dev;
    bdaddr_t                            addr;
    pthread_t                           thread_scan_pid;
    struct evhr_event_s *               ev_scan;
    MMB_ADAPTER_SCAN_CB                 cb_scan;
    struct mmb_adapter_scan_result_s *  scan_resp;
} MMB_ADAPTER;

int mmb_adapter_init(MMB_ADAPTER * this, bdaddr_t * addr);
int mmb_adapter_connect(MMB_ADAPTER * this, struct evhr_ctx_s * evhr);
int mmb_adapter_disconnect(MMB_ADAPTER * this);

int mmb_adapter_scan_start(MMB_ADAPTER * this, MMB_ADAPTER_SCAN_CB cb);
int mmb_adapter_scan_stop(MMB_ADAPTER * this);

#endif /* ifndef MMB_ADAPTER_H_ */
