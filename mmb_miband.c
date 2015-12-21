#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "mmb_ctx.h"
#include "evhr.h"

extern int mmb_ble_connect(const bdaddr_t * src, const bdaddr_t * dst);

int mmb_miband_start(MMB_CTX * mmb);
int mmb_miband_stop(MMB_CTX * mmb);

void timeout_cb(EVHR_EVENT * ev)
{
    printf("timeout\n");
    //mmb_miband_stop((MMB_CTX *)ev->pdata);
}

static void write_cb(EVHR_EVENT * ev)
{
    int ret;
    printf("write\n");
    uint8_t test[3] = {0x0A, 0x2c, 0x00};
    ret = write(ev->fd, test, 3);
    printf("write, ret[%d], errno[%d]\n", ret, errno);
}

static void read_cb(EVHR_EVENT * ev)
{
    
    int size, i;
    uint8_t buf[1024];
    size = read(ev->fd, buf, 1024);
    printf("read, size[%d], errno[%d]\n", size, errno);

    for (i = 0; i< size;i++)
        printf("%02x", buf[i]);

    printf("\n\n");
    
}

static void error_cb(EVHR_EVENT * ev)
{
    printf("Error\n");
    evhr_stop(((MMB_CTX *)ev->pdata)->evhr);
}

int mmb_miband_start(MMB_CTX * mmb)
{
    int sock = -1;

    // Create BLE connect
    if ((sock = mmb_ble_connect(&mmb->addr, &mmb->data.addr)) < 0)
    {
        printf("[MMB][MIBAND][ERR] Connect MiBand failed!\n");
        return -1;
    }

    // Bind BLE event into evhr
    if ((mmb->ev_ble = evhr_event_add_socket(
                    mmb->evhr, sock, 
                    mmb, read_cb, write_cb, error_cb)) == NULL)
    {
        printf("[MMB][MIBAND][ERR] Bind MiBand event failed!\n");
        mmb_miband_stop(mmb);
        return -2;
    }

    // Create timeout
    if ((mmb->timerfd = evhr_event_create_timer()) < 0)
    {
        printf("[MMB][MIBAND][ERROR] Create Timer failed!\n");
        mmb_miband_stop(mmb);
        return -3;
    }

    // Add timer into event handler
    if ((mmb->ev_timeout = evhr_event_add_timer_periodic(
            mmb->evhr, mmb->timerfd,
            MMB_MIBAND_TIMEOUT_SEC, 10, 
            mmb, timeout_cb)) == NULL)
    {
        printf("[MMB][MIBAND][ERROR] Bind Timer event failed!\n");
        mmb_miband_stop(mmb);
        return -4;
    }

    mmb->status = 1;

    return 0;
}

int mmb_miband_stop(MMB_CTX * mmb)
{
    if (mmb->ev_ble) {
        if (mmb->ev_ble->fd > 0) {
            close(mmb->ev_ble->fd);
        }
        evhr_event_del(mmb->evhr, mmb->ev_ble);
        mmb->ev_ble = NULL;
    }

    if (mmb->ev_timeout) {
        if (mmb->ev_timeout->fd > 0) {
            evhr_event_stop_timer(mmb->ev_timeout->fd);
            close(mmb->ev_timeout->fd);
        }
        evhr_event_del(mmb->evhr, mmb->ev_timeout);
        mmb->ev_timeout = NULL;
    }

    mmb->status = 0;

    return 0;
}

