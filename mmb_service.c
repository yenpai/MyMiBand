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
MMB_CTX g_mmb_ctx = NULL;

static void do_task_scan_resp(MMB_EVENT_DATA * data)
{
    EBleDevice device;
    char addr[18];

    MMB_CTX this = g_mmb_ctx;

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

static void do_task_scan_req(MMB_EVENT_DATA * data)
{
    int ret;
    MMB_CTX this = g_mmb_ctx;
    QList qlist = NULL;
    EBleDevice device = NULL;

    (void)data;

    MMB_LOG("[ScanTask]", "Running ...");

    // Create qlist for obtain scan result
    if ((ret = qlist_create(&qlist)) < 0)
    {
        MMB_LOG("[ScanTask]", "ERR: qlist_create failed! ret = %d.", ret);
        goto free_and_exit;
    }
    
    // Start Sacn 
    if ((ret = eble_adapter_scan(this->adapter, 5, qlist)) < 0)
    {
        MMB_LOG("[ScanTask]", "ERR: eble_adapter_scan failed! ret = %d.", ret);
        goto free_and_exit;
    }

    // shift all device in result, and send evnet to notify
    while ((device = qlist_shift(qlist)) != NULL)
        mmb_event_send(this->eventer, MMB_EVENT_SCAN_RESP, device, sizeof(device));

free_and_exit:

    if (qlist)
        qlist_free(qlist);
    
    MMB_LOG("[ScanTask]", "Finish");
}

static void event_handle_cb(MMB_EVENT_DATA * data, void * pdata)
{
    MMB_CTX this = pdata;

    switch (data->type)
    {
        default:
            MMB_DBG("[EVENT]", "Unknow event! type[0x%04x].", data->type);
            break;
        
        case MMB_EVENT_DUMMY:
            /* Nothing */
            break;

        case MMB_EVENT_SCAN_REQ:

            wpool_add(this->wpool, (WPOOL_JOB_FUNC) do_task_scan_req, data);
            break;

        case MMB_EVENT_SCAN_RESP:
            
            wpool_add(this->wpool, (WPOOL_JOB_FUNC) do_task_scan_resp, data);
            break;
    }

}

int mmb_service_init(bdaddr_t * bdaddr)
{
    int ret = 0;

    MMB_LOG("[SERVICE]","Initial proccess running ...");

    MMB_ASSERT(g_mmb_ctx == NULL);

    // Alloc memory and initial
    if ((g_mmb_ctx = malloc(sizeof(struct mmb_ctx_s))) == NULL)
    {
        MMB_LOG("[SERVICE]", "ERR: evhr_create failed! ret = %d", ret);
        return -1;
    }
    memset(g_mmb_ctx, 0, sizeof(mmb_ctx));

    // Inital EVHR
    MMB_DBG("[SERVICE]", "\t => EVHR");
    if ((ret = evhr_create(&g_mmb_ctx->evhr)) != EVHR_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: evhr_create failed! ret = %d", ret);
        return -1;
    }

    // Initial MMB Eventer
    MMB_DBG("[SERVICE]", "\t => Eventer");
    if ((ret = mmb_event_init(&g_mmb_ctx->eventer)) < 0)
    {
        MMB_LOG("[SERVICE]", "ERR: mmb_event_init failed! ret = %d", ret);
        return -2;
    }

    // Initial WorkerPool
    MMB_DBG("[SERVICE]", "\t => Worker Pool");
    if ((ret = wpool_create(&g_mmb_ctx->wpool, MMB_SERVICE_WPOOL_NUM)) != WPOOL_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: wpool_create failed! ret = %d", ret);
        return -3;
    }

    // Initial Devices
    MMB_DBG("[SERVICE]", "\t => Devices");
    if ((ret = qlist_create(&g_mmb_ctx->devices)) < 0)
    {
        MMB_LOG("[SERVICE]", "ERR: qlist_create failed! ret = %d", ret);
        return -4;
    }

    // Inital Adapter
    MMB_DBG("[SERVICE]", "\t => Adapter");
    if ((ret = eble_adapter_create(&g_mmb_ctx->adapter, bdaddr)) != EBLE_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: eble_adapter_create failed! ret = %d", ret);
        return -5;
    }

    // Reset Adapter
    MMB_DBG("[SERVICE]", "\t => Adapter(Reset)");
    if ((ret = eble_adapter_reset(g_mmb_ctx->adapter)) != EBLE_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: eble_adapter_reset failed! ret = %d", ret);
    }

    MMB_LOG("[SERVICE]", "\t => (Finish)");
    return 0;
}

int mmb_service_start()
{
    int ret;
    EBleDevice device;

    MMB_LOG("[SERVICE]", "Start proccess running ...");

    // Assert
    MMB_ASSERT(g_mmb_ctx);
    MMB_ASSERT(g_mmb_ctx->evhr);
    MMB_ASSERT(g_mmb_ctx->wpool);
    MMB_ASSERT(g_mmb_ctx->devices);
    MMB_ASSERT(g_mmb_ctx->eventer);
    MMB_ASSERT(g_mmb_ctx->adapter);

    MMB_DBG("[SERVICE]", "\t => Worker Pool(start)");
    if ((ret = wpool_start(g_mmb_ctx->wpool)) != WPOOL_RTN_SUCCESS)
    {
        MMB_LOG("[SERVICE]", "ERR: wpool_start failed! ret = %d", ret);
        return -1;
    }

    MMB_DBG("[SERVICE]", "\t => Into Service Main Loop");
    while (1)
    {
        MMB_DBG("[SERVICE]", "\t => Eventer(start)");
        if ((ret = mmb_event_start(g_mmb_ctx->eventer, g_mmb_ctx->evhr, event_handle_cb, g_mmb_ctx)) < 0)
        {
            MMB_LOG("[SERVIC]", "ERR: mmb_event_start failed! ret = %d.", ret);
            goto free_next_loop;
        }

        MMB_DBG("[SERVICE]", "\t => Adapter(connect)");
        if ((ret = eble_adapter_connect(g_mmb_ctx->adapter)) != EBLE_RTN_SUCCESS)
        {
            MMB_LOG("[SERVICE]", "ERR: mmb_adapter_connect failed! ret = %d.", ret);
            goto free_next_loop;
        }
        
        // Start scan here...
        MMB_DBG("[SERVICE]", "\t => Adapter(ScanReq)");
        mmb_event_send(g_mmb_ctx->eventer, MMB_EVENT_SCAN_REQ, NULL, 0);
        
        MMB_LOG("[SERVICE]", "\t => (Dispatch)");
        evhr_dispatch(g_mmb_ctx->evhr);

free_next_loop:

        MMB_LOG("[SERVICE]", "Stop process running ...");
        
        MMB_DBG("[SERVICE]", "\t => Adapter(Disconnect)");
        eble_adapter_disconnect(g_mmb_ctx->adapter);

        MMB_LOG("[SERVICE]", "\t => Devices(Free)");
        while ((device = qlist_shift(g_mmb_ctx->devices)) != NULL)
            eble_device_free(device);

        MMB_DBG("[SERVICE]", "\t => Eventer(Stop)");
        mmb_event_stop(g_mmb_ctx->eventer);

        MMB_DBG("[SERVICE]", "\t => Adapter(Reset)");
        if (eble_adapter_reset(g_mmb_ctx->adapter) == EBLE_RTN_FAILED)
            MMB_LOG("[SERVICE]", "\t => Adapter(Reset) failed!");
        
        MMB_LOG("[SERVICE]", "\t => (Restart service)");
    }

    return 0;
}

