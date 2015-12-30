#include <stdlib.h>
#include <unistd.h>

#include "evhr.h"
#include "qlist.h"
#include "eble.h"
#include "wpool.h"

#include "mmb_debug.h"
#include "mmb_ctx.h"
#include "mmb_event.h"
#include "mmb_miband.h"

#define MMB_SERVICE_WPOOL_NUM   5

static void do_event_scan_resp(MMB_CTX * this, MMB_EVENT_DATA * data)
{
    EBLE_DEVICE * device;
    char addr[18];

    MMB_ASSERT(this);
    MMB_ASSERT(this->devices);
    MMB_ASSERT(data);
    MMB_ASSERT(data->data);

    // Read Device Info
    device = data->data;
    ba2str(&device->addr, addr);

    MMB_DBG("[EVENT]", "SCAN RESP Device Name[%s], Addr[%s], Rssi[%d]",
            device->eir.LocalName, addr, device->rssi);

    // Probe, Init, Start, Push into devices
    if (mmb_miband_probe(device) == 0)
    {
        if (mmb_miband_init(&device) == 0)
        {
            if (mmb_miband_start((MMB_MIBAND *)device, this->adapter, this->evhr) == 0)
            {
                qlist_push(this->devices, device);
                return;
            }
        }
        
        goto free_and_exit;
    }

    // Unknow Device

free_and_exit:

    eble_device_free(device);
}

static void adapter_scan_cb(void * pdata, EBLE_DEVICE * device)
{
    MMB_ASSERT(pdata);
    MMB_ASSERT(device);

    MMB_DBG("[Adapter]", "Notify service have new device after scan.");
    MMB_CTX * this = pdata;
    mmb_event_send(this->eventer, MMB_EVENT_SCAN_RESP, device, sizeof(device));
}

static void event_handle_cb(MMB_EVENT_DATA * data, void * pdata)
{
    int ret;
    MMB_CTX * this = pdata;

    switch (data->type)
    {
        default:
            MMB_DBG("[EVENT]", "Unknow event! type[0x%04x].", data->type);
            break;
        
        case MMB_EVENT_DUMMY:
            /* Nothing */
            break;

        case MMB_EVENT_SCAN_REQ:
            if ((ret = eble_adapter_scan(this->adapter, 5, adapter_scan_cb, this)) < 0)
                MMB_LOG("[EVENT]", "ERR: eble_adapter_scan failed! ret = %d.", ret);
            break;

        case MMB_EVENT_SCAN_RESP:
            do_event_scan_resp(this, data);
            break;

    }

}

int mmb_service_init(MMB_CTX * this, bdaddr_t * bdaddr)
{
    int ret = 0;

    MMB_LOG("[SERVICE]","Initial proccess running ...");

    // Default config
    memset(this, 0, sizeof(MMB_CTX));

    // Inital EVHR
    MMB_DBG("[SERVICE]", "\t => EVHR");
    if ((ret = evhr_create(&this->evhr)) != EVHR_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: evhr_create failed! ret = %d", ret);
        return -1;
    }

    // Initial MMB Eventer
    MMB_DBG("[SERVICE]", "\t => Eventer");
    if ((ret = mmb_event_init(&this->eventer)) < 0)
    {
        MMB_LOG("[SERVICE]", "ERR: mmb_event_init failed! ret = %d", ret);
        return -2;
    }

    // Initial WorkerPool
    MMB_DBG("[SERVICE]", "\t => Worker Pool");
    if ((ret = wpool_create(&this->wpool, MMB_SERVICE_WPOOL_NUM)) != WPOOL_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: wpool_create failed! ret = %d", ret);
        return -3;
    }

    // Initial Devices
    MMB_DBG("[SERVICE]", "\t => Devices");
    if ((ret = qlist_create(&this->devices)) < 0)
    {
        MMB_LOG("[SERVICE]", "ERR: qlist_create failed! ret = %d", ret);
        return -4;
    }

    // Inital Adapter
    MMB_DBG("[SERVICE]", "\t => Adapter");
    if ((ret = eble_adapter_create(&this->adapter, bdaddr)) != EBLE_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: eble_adapter_create failed! ret = %d", ret);
        return -5;
    }

    // Reset Adapter
    MMB_DBG("[SERVICE]", "\t => Adapter(Reset)");
    if ((ret = eble_adapter_reset(this->adapter)) != EBLE_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: eble_adapter_reset failed! ret = %d", ret);
    }

    MMB_LOG("[SERVICE]", "\t => (Finish)");
    return 0;
}

int mmb_service_start(MMB_CTX * this)
{
    int ret;
    EBLE_DEVICE * device;

    MMB_LOG("[SERVICE]", "Start proccess running ...");

    // Assert
    MMB_ASSERT(this);
    MMB_ASSERT(this->evhr);
    MMB_ASSERT(this->wpool);
    MMB_ASSERT(this->devices);
    MMB_ASSERT(this->eventer);
    MMB_ASSERT(this->adapter);

    MMB_DBG("[SERVICE]", "\t => Worker Pool(start)");
    if ((ret = wpool_start(this->wpool)) != WPOOL_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: wpool_start failed! ret = %d", ret);
        return -1;
    }

    MMB_DBG("[SERVICE]", "\t => Into Service Main Loop");
    while (1)
    {
        MMB_DBG("[SERVICE]", "\t => Eventer(start)");
        if ((ret = mmb_event_start(this->eventer, this->evhr, event_handle_cb, this)) < 0)
        {
            MMB_LOG("[SERVIC]", "ERR: mmb_event_start failed! ret = %d.", ret);
            goto free_next_loop;
        }

        MMB_DBG("[SERVICE]", "\t => Adapter(connect)");
        if ((ret = eble_adapter_connect(this->adapter)) != EBLE_RTN_SUCCESS)
        {
            MMB_LOG("[SERVICE]", "ERR: mmb_adapter_connect failed! ret = %d.", ret);
            goto free_next_loop;
        }
        
        // Start scan here...
        MMB_DBG("[SERVICE]", "\t => Adapter(ScanReq)");
        mmb_event_send(this->eventer, MMB_EVENT_SCAN_REQ, NULL, 0);
        
        MMB_LOG("[SERVICE]", "\t => (Dispatch)");
        evhr_dispatch(this->evhr);

free_next_loop:

        MMB_LOG("[SERVICE]", "Stop process running ...");
        
        MMB_DBG("[SERVICE]", "\t => Adapter(Disconnect)");
        eble_adapter_disconnect(this->adapter);

        MMB_LOG("[SERVICE]", "\t => Devices(Free)");
        while ((device = qlist_shift(this->devices)) != NULL)
            eble_device_free(device);

        MMB_DBG("[SERVICE]", "\t => Eventer(Stop)");
        mmb_event_stop(this->eventer);

        MMB_DBG("[SERVICE]", "\t => Adapter(Reset)");
        if (eble_adapter_reset(this->adapter) == EBLE_RTN_FAILED)
            MMB_LOG("[SERVICE]", "\t => Adapter(Reset) failed!");
        
        MMB_LOG("[SERVICE]", "\t => (Restart service)");
    }

    return 0;
}

