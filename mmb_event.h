#ifndef MMB_EVENT_H_
#define MMB_EVENT_H_

#include <stdint.h>
#include <stdlib.h>

#include "evhr.h"

typedef enum {
    MMB_EVENT_DUMMY        = 0x0000,
    MMB_EVENT_SCAN_REQ     = 0x0201,
    MMB_EVENT_SCAN_RESP    = 0x0202,
} MMB_EVENT_TYPE;

typedef struct mmb_event_data_s {
    MMB_EVENT_TYPE type;
    size_t      size;
    void *      data; 
} __attribute__((packed)) MMB_EVENT_DATA;

typedef void (*MMB_EVENT_CB) (MMB_EVENT_DATA *, void *pdata);

typedef struct mmb_event_ctx_s {
    int status;
    int sfd[2];
    EVHR_EVENT * ev;
    MMB_EVENT_CB cb;
    void *       cb_pdata;
} MMB_EVENT;

int mmb_event_init(MMB_EVENT ** this);
int mmb_event_start(MMB_EVENT * this, EVHR_CTX * evhr, MMB_EVENT_CB cb, void * cb_pdata);
int mmb_event_stop(MMB_EVENT * this);

int mmb_event_send(MMB_EVENT * this, MMB_EVENT_TYPE type, void * data, size_t size);

#endif
