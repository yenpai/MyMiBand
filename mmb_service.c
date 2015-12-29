#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "evhr.h"
#include "qlist.h"
#include "eble.h"

#include "mmb_ctx.h"
#include "mmb_event.h"
#include "mmb_miband.h"

static void do_event_scan_resp(MMB_CTX * this, MMB_EVENT_DATA * data)
{
    EBLE_DEVICE * device;
    char addr[18];

    assert(this && this->devices);
    assert(data && data->buf);

    // Read Device Info
    device = data->buf;
    ba2str(&device->addr, addr);
    printf("[MMB][EVENT][SCAN][RESP] Device Name[%s], Addr[%s], Rssi[%d]\n", 
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
    MMB_CTX * this = pdata;
    mmb_event_send(this->eventer, MMB_EV_SCAN_RESP, device, sizeof(device));
}

static void event_handle_cb(MMB_EVENT_DATA * data, void * pdata)
{
    int ret;
    MMB_CTX * this = pdata;

    switch (data->type)
    {
        case MMB_EV_SCAN_REQ:
            printf("[MMB][EVENT] MMB_EV_SCAN_REQ.\n");
            if ((ret = eble_adapter_scan(this->adapter, 5, adapter_scan_cb, this)) < 0)
                printf("[MMB][EVENT] ERR: eble_adapter_scan failed! ret = %d.\n", ret);
            break;

        case MMB_EV_SCAN_RESP:
            printf("[MMB][EVENT] MMB_EV_SCAN_RESP.\n");
            do_event_scan_resp(this, data);
            break;

        case MMB_EV_UNKNOW:
        default:
            printf("[MMB][EVENT] Unknow event! type[0x%04x].", data->type);
            break;
    }

}

int mmb_service_init(MMB_CTX * this, bdaddr_t * addr)
{
    int ret = 0;

    // Default config
    memset(this, 0, sizeof(MMB_CTX));

    // Inital EVHR
    if ((ret = evhr_create(&this->evhr)) != EVHR_RTN_SUCCESS)
    {
        printf("[MMB][SERVICE] ERR: evhr_create failed! ret = %d\n", ret);
        return -1;
    }

    // Initial MMB Eventer
    if ((ret = mmb_event_init(&this->eventer)) < 0)
    {
        printf("[MMB][SERVICE] ERR: mmb_event_init failed! ret = %d\n", ret);
        return -2;
    }

    // Inital Adapter
    if ((ret = eble_adapter_create(&this->adapter, addr)) != EBLE_RTN_SUCCESS)
    {
        printf("[MMB][SERVICE] ERR: eble_adapter_create failed! ret = %d\n", ret);
        return -3;
    }

    // Initial Devices
    if ((ret = qlist_create(&this->devices)) < 0)
    {
        printf("[MMB][SERVICE] ERR: qlist_create failed! ret = %d\n", ret);
        return -4;
    }

    printf("[MMB][SERVICE] initial.\n");

    return 0;
}

int mmb_service_start(MMB_CTX * this)
{
    int ret;
    EBLE_DEVICE * device;

    printf("[MMB][SERVICE] start.\n");

    // Assert
    assert(this);
    assert(this->devices);
    assert(this->eventer);
    assert(this->adapter);

    while (1)
    {
        printf("[MMB][SERVICE] restart adapter.\n");
        eble_adapter_reset(this->adapter);

        if ((ret = mmb_event_start(this->eventer, this->evhr, event_handle_cb, this)) < 0)
        {
            printf("[MMB][SERVICE] ERR: mmb_event_start failed! ret = %d.\n", ret);
            goto free_next_loop;
        }

        if ((ret = eble_adapter_connect(this->adapter)) < 0)
        {
            printf("[MMB][SERVICE] ERR: mmb_adapter_connect failed! ret = %d.\n", ret);
            goto free_next_loop;
        }
        
        // Start scan here...
        mmb_event_send(this->eventer, MMB_EV_SCAN_REQ, NULL, 0);

        printf("[MMB][SERVICE] running.\n");
        evhr_dispatch(this->evhr);

free_next_loop:

        printf("[MMB][SERVICE] stop.\n");
        
        eble_adapter_disconnect(this->adapter);

        while (this->devices->counts > 0)
        {
            if ((device = qlist_shift(this->devices)) != NULL)
                eble_device_free(device);
        }

        mmb_event_stop(this->eventer);

    }

    return 0;
}

