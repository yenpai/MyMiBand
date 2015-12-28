#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "evhr.h"
#include "mmb_util.h"
#include "mmb_event.h"
#include "mmb_adapter.h"
#include "mmb_ble.h"

#define SCAN_TIMER_INTERVAL_SEC         30
#define SCAN_WAIT_SEC_BEFORE_READ       3
#define SCAN_READ_TIME_SEC              2

static void thread_scan_main(void * pdata)
{
    int ret, i;
    MMB_ADAPTER * this = pdata;
    struct mmb_ble_advertising_s * adv = NULL;
    
    printf("[MMB][SCAN][THREAD] start.\n");
    
    // Start
    if ((ret = mmb_ble_scan_start(this->dev)) < 0)
    {
        printf("[MMB][SCAN][THREAD] ERR: ble_scan_start failed! ret = %d.\n", ret);
        goto thread_exit;
    }

    // Waiting..
    sleep(SCAN_WAIT_SEC_BEFORE_READ);

    // Reading
    for (i=0;i<SCAN_READ_TIME_SEC*2;i++)
    {
        mmb_ble_scan_reader(this->dev, this->scan_results);
        usleep(500000);
    }
    
    printf("[MMB][SCAN][THREAD] stop, counts = %lu\n", this->scan_results->counts);
    
    // Flush all results and send notify to mmb_event
    while ((adv = qlist_shift(this->scan_results)) != NULL)
    {
        if (this->scan_notify_eventer)
        {
            mmb_event_send(this->scan_notify_eventer, 
                    MMB_EV_SCAN_RESP, adv, sizeof(struct mmb_ble_advertising_s));
        }
    }


thread_exit:

    // restart a timer to invoke next scan
    if (this->ev_scan_timer)
        evhr_event_set_timer(this->ev_scan_timer, SCAN_TIMER_INTERVAL_SEC, 0, 1);
    
    printf("[MMB][SCAN][THREAD] exit.\n");
}

static void scan_timer_cb(EVHR_EVENT * ev)
{
    int ret;
    MMB_ADAPTER * this = ev->pdata;

    // Start a thread to read result
    if ((ret = pthread_create(&this->thread_scan_pid, NULL, (void *) thread_scan_main, this)) != 0)
    {
        printf("[MMB][SCAN] ERR: create scan thread failed! ret = %d.\n", ret);
        return;
    }
}

int mmb_adapter_scan_stop(MMB_ADAPTER * this)
{
    struct mmb_ble_device_base_s * device = NULL;

    if (this->ev_scan_timer)
    {
        evhr_event_del(this->ev_scan_timer);
        this->ev_scan_timer = NULL;
    }
    
    if (this->thread_scan_pid > 0)
    {
        pthread_cancel(this->thread_scan_pid);
        pthread_join(this->thread_scan_pid, NULL);    
        this->thread_scan_pid = 0;
    }
    
    if (this->scan_results) {
        while ((device = qlist_shift(this->scan_results)) != NULL)
            free(device);
    }

    printf("[MMB][SCAN] stop.\n");
    return 0;
}

int mmb_adapter_scan_start(MMB_ADAPTER * this, EVHR_CTX * evhr, struct mmb_event_ctx_s * eventer)
{

    if (this->thread_scan_pid > 0)
    {
        printf("[MMB][SCAN] ERR: scan thread running, can not start!\n");
        return -1;
    }

    this->scan_notify_eventer = eventer;

    if (this->scan_results == NULL)
    {
        if (qlist_create(&this->scan_results) < 0)
        {
            printf("[MMB][SCAN] ERR: qlist_create failed!\n");
            return -2;
        }
    }

    if ((this->ev_scan_timer = evhr_event_add_timer_once(
            evhr, 0, 0, this, scan_timer_cb)) == NULL)
    {
        printf("[MMB][SCAN] add scan_timer_event failed!\n");
        return -3;
    }

    printf("[MMB][SCAN] start.\n");

    return 0;
}

int mmb_adapter_disconnect(MMB_ADAPTER * this)
{

    mmb_adapter_scan_stop(this);
    
    if (this->dev > 0)
    {
        hci_close_dev(this->dev);
        this->dev = 0;
    }

    int dev_id;
    char cmd[64];

    dev_id = hci_get_route(&this->addr);
    sprintf(cmd, "hciconfig hci%d down && hciconfig hci%d up", dev_id, dev_id);
    
    system(cmd);

    return 0;
}

int mmb_adapter_connect(MMB_ADAPTER * this)
{
    int dev_id;

    // open device
    dev_id = hci_get_route(&this->addr);
    if ((this->dev = hci_open_dev(dev_id)) < 0)
    {
        printf("[MMB][ADAPTER] open adapter failed! ret = %d.\n", this->dev);
        return -1;
    }

    // For scan read, nonblocking mode
    if (socket_setting_non_blocking(this->dev) < 0)
    {
        printf("[MMB][ADAPTER] setting non blocking mode failed!\n");
        return -2;
    }

    printf("[MMB][ADAPTER] connected.\n");

    return 0;
}

int mmb_adapter_init(MMB_ADAPTER * this, bdaddr_t * addr)
{
    memset(this, 0, sizeof(MMB_ADAPTER));
    bacpy(&this->addr, addr);
    
    return 0;
}
