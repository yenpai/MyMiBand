#include <unistd.h>
#include <stdlib.h>

#include "evhr.h"

#include "mmb_ctx.h"
#include "mmb_ble.h"
#include "mmb_miband.h"

static void scan_timer_cb(EVHR_EVENT * ev)
{
    int ret;
    MMB_CTX * this = ev->pdata;
    //uint8_t buf[MMB_BUFFER_SIZE];

    printf("scan1\n");
    if ((ret = mmb_ble_scan_reader(this->adapter_dev)) < 0)
    {
        printf("scan error, ret = %d\n", ret);
        return;
    }
    printf("scan2\n");
    evhr_event_set_timer(ev->fd, 5, 0, 1);
}

static int mmb_service_scan_stop(MMB_CTX * this)
{
    int ret;

    if (this->adapter_dev > 0)
    {
        ret = mmb_ble_scan_stop(this->adapter_dev, 1000);
        if (ret < 0)
            printf("scan_stop failed! ret = %d\n", ret);

        hci_close_dev(this->adapter_dev);
        this->adapter_dev = 0;
    }
    
    if (this->ev_scan_timer) {
        evhr_event_stop_timer(this->ev_scan_timer->fd);
        close(this->ev_scan_timer->fd);
        evhr_event_del(this->ev_scan_timer);
        this->ev_scan_timer = NULL;
    }

    printf("[MMB][SCAN] stop.\n");
    return 0;
}

static int mmb_service_scan_start(MMB_CTX * this)
{
    /* test scan */
    char addr[18];
    int ret, timerfd;
    
    // open device
    ba2str(&this->addr, addr);
    this->adapter_dev = hci_open_dev(hci_devid(addr));
    if (this->adapter_dev < 0) {
        printf("open adapter failed! ret = %d\n", this->adapter_dev);
        return -1;
    }

    // scan start
    ret = mmb_ble_scan_start(this->adapter_dev, 5000);
    if (ret < 0) {
        printf("scan_start failed! ret = %d\n", ret);
        mmb_service_scan_stop(this);
        return -2;
    }
    
    printf("[MMB][SCAN] start.\n");
    
    timerfd = evhr_event_create_timer();

    // Add event handler
    this->ev_scan_timer = evhr_event_add_timer_once(
            this->evhr, timerfd, 0, 10, 
            this, scan_timer_cb);
    if (this->ev_scan_timer == NULL) {
        printf("add event failed! ret = %d\n", ret);
        mmb_service_scan_stop(this);
        return -3;
    }

    return 0;
}


int mmb_service_init_miband(MMB_CTX * this, bdaddr_t * dest)
{
    int ret = 0;

    // Inital MIBAND
    if ((ret = mmb_miband_init(this->miband, dest, this->evhr)) < 0)
    {
        printf("[MMB][SERVICE] ERR: mmb_miband_init failed! ret = %d\n", ret);
        return -1;
    }
    
    printf("[MMB][SERVICE] miband initial.\n");

    return 0;
}

int mmb_service_init(MMB_CTX * this, bdaddr_t * ble_adapter_addr)
{
    int ret = 0;

    // Default config
    memset(this, 0, sizeof(MMB_CTX));
    bacpy(&this->addr, ble_adapter_addr);
    this->miband = (MMB_MIBAND *) malloc(sizeof(MMB_MIBAND));

    // Inital EVHR
    if ((ret = evhr_create(&this->evhr)) != EVHR_RTN_SUCCESS)
    {
        printf("[MMB][SERVICE] ERR: evhr_create failed! ret = %d\n", ret);
        return -1;
    }

    printf("[MMB][SERVICE] initial.\n");

    return 0;
}

int mmb_service_start(MMB_CTX * this)
{
    printf("[MMB][SERVICE] start.\n");

    while (1)
    {
        if (mmb_service_scan_start(this) < 0)
        {
            printf("[MMB][SERVICE][ERR] mmb_service_scan_start failed!\n");
            goto free_next_loop;
        }

        if (mmb_miband_start(this->miband, &this->addr) < 0)
        {
            printf("[MMB][SERVICE][ERR] mmb_miband_start failed!\n");
            goto free_next_loop;
        }

        printf("[MMB][SERVICE] running.\n");
        evhr_dispatch(this->evhr);

free_next_loop:

        printf("[MMB][SERVICE] stop.\n");
        mmb_miband_stop(this->miband);
        mmb_service_scan_stop(this);
    }

    return 0;
}

