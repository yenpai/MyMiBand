#include <stdlib.h>
#include <unistd.h>

#include "evhr.h"
#include "qlist.h"

#include "mmb_ctx.h"
#include "mmb_event.h"
#include "mmb_adapter.h"
#include "mmb_device.h"
#include "mmb_miband.h"

#if 0
static void adapter_scan_cb(void *pdata, struct mmb_adapter_scan_result_s * results, int size)
{
    int ret;
    MMB_CTX * this = pdata;
    
    struct mmb_adapter_scan_result_s * result = NULL;
    char addr[18];

    MMB_DEVICE * device = NULL;
    void * device_ctx   = NULL;

    if (size <= 0)
        return;

    printf("[MMB][SCAN][CB] Find BLE Device: counts = %d\n", size);

    for (result = results; result; result = result->next)
    {

        ba2str(&result->addr, addr);
        printf("[MMB][SCAN][CB] \t==> Name[%s], Addr[%s], Rssi[%d]\n", result->name, addr, result->rssi);

        if (mmb_miband_probe(result) == 0)
        {

            // Create DeviceCtx (MiBand)
            if ((device_ctx = (void *) malloc(sizeof(struct mmb_miband_ctx_s))) == NULL)
            {
                printf("[MMB][SCAN][CB] \t\t==> ERR: malloc failed!\n");
                continue;
            }
            
            // Init MIBAND
            if ((ret = mmb_miband_init(device_ctx, &result->addr, this->evhr)) < 0)
            {
                printf("[MMB][SCAN][CB] \t\t==> ERR: mmb_miband_init failed! ret[%d]", ret);
                free(device_ctx);
                continue;
            }
            
            // Create Device
            if ((device = malloc(sizeof(MMB_DEVICE))) == NULL)
            {
                printf("[MMB][SCAN][CB] \t\t==> ERR: malloc failed!\n");
                free(device_ctx);
                continue;
            }

            // Init Device
            strncpy(device->name, result->name, sizeof(device->name));
            bacpy(&device->addr, &result->addr);
            device->device_ctx = device_ctx;

            // Start DeviceCtx (MiBand)
            printf("[MMB][SCAN][CB] \t\t==> Start MIBAND.\n");
            if ((ret = mmb_miband_start(device->device_ctx, &this->adapter->addr)) < 0)
            {
                printf("[MMB][SCAN][CB] \t\t==> ERR: mmb_miband_start failed! ret = %d.\n", ret);
                free(device_ctx);
                free(device);
                continue;
            }

            // Push device into list
            qlist_push(this->devices, device);
            continue;
        }

    }

}
#endif

static int event_do_scan_resp(MMB_CTX * this, MMB_EVENT_DATA * data)
{
    int ret;
    struct mmb_ble_device_base_s * base = NULL;
    MMB_DEVICE * device = NULL;
    void * device_ctx   = NULL;
    char addr[18];

    if (data->size < sizeof(struct mmb_ble_device_base_s))
        return -1;

    base = data->buf;
    ba2str(&base->addr, addr);
    printf("[MMB][EVENT] ==> Name[%s], Addr[%s], Rssi[%d]\n", base->name, addr, base->rssi);

    if (mmb_miband_probe(data->buf) == 0)
    {
        printf("[MMB][EVENT] ==> MIBAND Device.\n");
        // Create DeviceCtx (MiBand)
        if ((device_ctx = (void *) malloc(sizeof(struct mmb_miband_ctx_s))) == NULL)
        {
            printf("[MMB][EVENT] ==> ERR: malloc failed!\n");
            return -2;
        }

        // Init MIBAND
        if ((ret = mmb_miband_init(device_ctx, &base->addr, this->evhr)) < 0)
        {
            printf("[MMB][EVENT] ==> ERR: mmb_miband_init failed! ret[%d]", ret);
            free(device_ctx);
            return -2;
        }

        // Create Device
        if ((device = malloc(sizeof(MMB_DEVICE))) == NULL)
        {
            printf("[MMB][EVENT] \t\t==> ERR: malloc failed!\n");
            free(device_ctx);
            return -2;
        }

        // Init Device
        strncpy(device->name, base->name, sizeof(device->name));
        bacpy(&device->addr, &base->addr);
        device->device_ctx = device_ctx;

        // Start DeviceCtx (MiBand)
        printf("[MMB][EVENT] ==> Start MIBAND.\n");
        if ((ret = mmb_miband_start(device->device_ctx, &this->adapter->addr)) < 0)
        {
            printf("[MMB][EVENT] \t\t==> ERR: mmb_miband_start failed! ret = %d.\n", ret);
            free(device_ctx);
            free(device);
            return -2;
        }

        // Push device into list
        qlist_push(this->devices, device);

        return 0;
    }

    printf("[MMB][EVENT] ==> Unknow device.\n");
    return -99;
}

static void event_handle_cb(MMB_EVENT_DATA * data, void *pdata)   
{
    int ret;
    MMB_CTX * this = pdata;

    switch (data->type)
    {
        case MMB_EV_SCAN_REQ:

            printf("[MMB][EVENT] MMB_EV_SCAN_REQ.\n");
            if ((ret = mmb_adapter_scan_start(this->adapter, this->evhr, this->eventer)) < 0)
                printf("[MMB][EVENT] ERR: mmb_adapter_scan_start failed! ret = %d.\n", ret);

            break;

        case MMB_EV_SCAN_RESP:

            printf("[MMB][EVENT] MMB_EV_SCAN_RESP.\n");
            event_do_scan_resp(this, data);

            break;

        case MMB_EV_UNKNOW:
        default:

            printf("[MMB][EVENT] Unknow event! type[0x%04x].", data->type);

            break;
    }

    if (data->size > 0 && data->buf)
        free(data->buf);

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

    // Initial Devices
    if ((ret = qlist_create(&this->devices)) < 0)
    {
        printf("[MMB][SERVICE] ERR: qlist_create failed! ret = %d\n", ret);
        return -2;
    }

    // Inital Adapter
    if ((this->adapter = malloc(sizeof(MMB_ADAPTER))) == NULL)
    {
        printf("[MMB][SERVICE] ERR: malloc failed! ret = %d\n", ret);
        return -3;
    }
    else if ((ret = mmb_adapter_init(this->adapter, addr)) < 0)
    {
        printf("[MMB][SERVICE] ERR: mmb_adapter_init failed! ret = %d\n", ret);
        return -4;
    }

    printf("[MMB][SERVICE] initial.\n");

    return 0;
}

int mmb_service_start(MMB_CTX * this)
{
    int ret;
    
    MMB_DEVICE * device;

    printf("[MMB][SERVICE] start.\n");


    while (1)
    {
        if ((ret = mmb_event_start(this->eventer, this->evhr, event_handle_cb, this)) < 0)
        {
            printf("[MMB][SERVICE] ERR: mmb_event_start failed! ret = %d.\n", ret);
            goto free_next_loop;
        }

        if ((ret = mmb_adapter_connect(this->adapter)) < 0)
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
        
        mmb_adapter_scan_stop(this->adapter);
        mmb_adapter_disconnect(this->adapter);

        while (this->devices->counts > 0)
        {
            if ((device = qlist_shift(this->devices)) != NULL)
            {
                mmb_miband_stop(device->device_ctx);
                free(device->device_ctx);
                free(device);
            }
        }

        mmb_event_stop(this->eventer);
    }

    return 0;
}

