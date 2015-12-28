#ifndef MMB_DEVICE_H_
#define MMB_DEVICE_H_

#include <bluetooth/bluetooth.h>

typedef struct mmb_device_s {
    bdaddr_t                    addr;
    char                        name[32];
    void *                      device_ctx;
} MMB_DEVICE;

#endif
