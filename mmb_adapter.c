#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#include "evhr.h"
#include "mmb_util.h"
#include "mmb_adapter.h"
#include "mmb_ble.h"

#define SCAN_TIMER_INTERVAL_SEC         30
#define SCAN_READ_TIMEOUT_SEC           5

static void thread_scan_main(void * pdata)
{
    int ret, i;
    MMB_ADAPTER * this = pdata;
    uint64_t notify = 1;
    
    printf("[MMB][SCAN][THREAD] start.\n");
    
    this->scan_resp = NULL;
    this->scan_resp_counts = 0;
    
    // Start
    if ((ret = mmb_ble_scan_start(this->dev)) < 0)
    {
        printf("[MMB][SCAN][THREAD] ERR: ble_scan_start failed! ret = %d.\n", ret);
        goto thread_exit;
    }

    // Read, default scan 5 sec
    for (i=0;i<SCAN_READ_TIMEOUT_SEC;i++)
    {
        this->scan_resp_counts += mmb_ble_scan_reader(this->dev, &this->scan_resp);
        sleep(1);
    }
    
    // Stop
    if ((ret = mmb_ble_scan_stop(this->dev)) < 0)
    {
        printf("[MMB][SCAN][THREAD] ERR: ble_scan_stop failed! ret = %d.\n", ret);
        goto thread_exit;
    }

    printf("[MMB][SCAN][THREAD] stop.\n");

thread_exit:

    printf("[MMB][SCAN][THREAD] exit.\n");
    write(this->ev_scan_notify->fd, &notify, sizeof(notify));
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

static void scan_notify_cb(EVHR_EVENT * ev)
{
    MMB_ADAPTER * this = ev->pdata;

    // Scan Callback
    if (this->cb_scan)
        this->cb_scan(this->cb_scan_pdata, this->scan_resp, this->scan_resp_counts);

    // Free results
    if (this->scan_resp)
    {
        mmb_ble_scan_results_free(this->scan_resp);
        this->scan_resp = NULL;
        this->scan_resp_counts = 0;
    }

    // Wait thread stop
    if (this->thread_scan_pid > 0)
    {
        pthread_cancel(this->thread_scan_pid);
        pthread_join(this->thread_scan_pid, NULL);    
        this->thread_scan_pid = 0;
    }

    // restart a timer to invoke next scan
    if (this->ev_scan_timer)
        evhr_event_set_timer(this->ev_scan_timer, SCAN_TIMER_INTERVAL_SEC, 0, 1);
}

int mmb_adapter_scan_stop(MMB_ADAPTER * this)
{
    if (this->thread_scan_pid > 0)
    {
        pthread_cancel(this->thread_scan_pid);
        pthread_join(this->thread_scan_pid, NULL);    
        this->thread_scan_pid = 0;
    }
    
    if (this->scan_resp)
    {
        mmb_ble_scan_results_free(this->scan_resp);
        this->scan_resp = NULL;
        this->scan_resp_counts = 0;
    }

    if (this->ev_scan_timer)
    {
        evhr_event_del(this->ev_scan_timer);
        this->ev_scan_timer = NULL;
    }
    
    printf("[MMB][SCAN] stop.\n");
    return 0;
}

int mmb_adapter_scan_start(MMB_ADAPTER * this, EVHR_CTX * evhr, MMB_ADAPTER_SCAN_CB cb, void *pdata)
{

    if (this->thread_scan_pid > 0)
    {
        printf("[MMB][SCAN] ERR: scan thread running, can not start!\n");
        return -1;
    }

    this->cb_scan = cb;
    this->cb_scan_pdata = pdata;

    if ((this->ev_scan_timer = evhr_event_add_timer_once(
            evhr, 0, 10, this, scan_timer_cb)) == NULL)
    {
        printf("[MMB][ADAPTER][SCAN] add scan_timer_event failed!\n");
        return -2;
    }

    printf("[MMB][SCAN] start.\n");

    return 0;
}

int mmb_adapter_disconnect(MMB_ADAPTER * this)
{
    if (this->ev_scan_timer) {
        evhr_event_del(this->ev_scan_timer);
        this->ev_scan_timer = NULL;
    }

    if (this->ev_scan_notify) {
        evhr_event_del(this->ev_scan_notify);
        this->ev_scan_notify = NULL;
    }
    
    if (this->dev > 0)
    {
        hci_close_dev(this->dev);
        this->dev = 0;
    }

    return 0;
}

int mmb_adapter_connect(MMB_ADAPTER * this, EVHR_CTX * evhr)
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

    // Add a counter to listen scan thread result
    if ((this->ev_scan_notify = evhr_event_add_counter(
            evhr, 0, this, scan_notify_cb)) == NULL)
    {
        printf("[MMB][ADAPTER][SCAN] add scan_notify_event failed!\n");
        return -3;
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
