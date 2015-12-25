#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "evhr.h"
#include "mmb_util.h"
#include "mmb_adapter.h"
#include "mmb_ble.h"

static void thread_scan_main(void * pdata)
{
    int ret;
    MMB_ADAPTER * this = pdata;
    uint64_t notify = 1;
    
    printf("[MMB][SCAN][THREAD] start.\n");

    // Start
    if ((ret = mmb_ble_scan_start(this->dev)) < 0)
    {
        printf("[MMB][SCAN][THREAD] ble_scan_start failed! ret = %d.\n", ret);
        goto thread_exit;
    }

    // Read
    ret = mmb_ble_scan_reader(this->dev);
    if (ret <= 0)
    {
        // No Data
    }
    else
    {
        // Update counter to notify main thread have new scan result
    }
    
    // Stop
    if ((ret = mmb_ble_scan_stop(this->dev)) < 0)
    {
        printf("[MMB][SCAN][THREAD] ble_scan_stop failed! ret = %d.\n", ret);
        goto thread_exit;
    }

    printf("[MMB][SCAN][THREAD] stop.\n");

thread_exit:

    write(this->ev_scan->fd, &notify, sizeof(notify));
    printf("[MMB][SCAN][THREAD] exit.\n");

}

static void scan_result_cb(EVHR_EVENT * ev)
{
    //int ret;
    MMB_ADAPTER * this = ev->pdata;
    //uint8_t buf[MMB_BUFFER_SIZE];

    // Read Scan Result
    printf("have scan result.\n");

    // Scan Callback
    if (this->cb_scan)
        this->cb_scan(this->scan_resp, 0);

    // TODO: free scan results
    //if (this->scan_resp)
       

    // TODO: start a timer to invoke next scan
}


int mmb_adapter_scan_stop(MMB_ADAPTER * this)
{
    pthread_cancel(this->thread_scan_pid);
    pthread_join(this->thread_scan_pid, NULL);    
    
    printf("[MMB][SCAN] stop.\n");
    return 0;
}

int mmb_adapter_scan_start(MMB_ADAPTER * this, MMB_ADAPTER_SCAN_CB cb)
{
    int ret;

    this->cb_scan = cb;
    this->scan_resp = NULL;

    // Start a thread to read result
    if ((ret = pthread_create(&this->thread_scan_pid, NULL, (void *) thread_scan_main, this)) != 0)
    {
        printf("[MMB][ADAPTER] create scan thread failed! ret = %d.\n", ret);
        return -1;
    }

    printf("[MMB][SCAN] start.\n");

    return 0;
}

int mmb_adapter_disconnect(MMB_ADAPTER * this)
{
    if (this->dev > 0)
    {
        hci_close_dev(this->dev);
        this->dev = 0;
    }

    if (this->ev_scan) {
        evhr_event_del(this->ev_scan);
        this->ev_scan = NULL;
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
        printf("[MMB][ADAPTER][SCAN] open adapter failed! ret = %d.\n", this->dev);
        return -1;
    }

    // Add a counter to listen scan thread result
    if ((this->ev_scan = evhr_event_add_counter(
            evhr, 0, this, scan_result_cb)) == NULL)
    {
        printf("[MMB][ADAPTER][SCAN] add counter failed!\n");
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
