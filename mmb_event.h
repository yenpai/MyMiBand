#ifndef MMB_EVENT_H_
#define MMB_EVENT_H_

#include <stdint.h>

#define MMB_EV_UNKNOW               0x0000
#define MMB_EV_SCAN_REQ             0x0201
#define MMB_EV_SCAN_RESP            0x0202

typedef struct mmb_event_data_s {
    uint8_t     type;
    uint8_t     length;
    void *      pdata; 
} MMB_EVENT_DATA;

typedef struct mmb_event_ctx_s {

} MMB_EVENT;

int mmb_event_init();
int mmb_event_start();
int mmb_event_stop();

int mmb_event_send();

#endif
