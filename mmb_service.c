#include <stdlib.h>
#include <unistd.h>

#include "evhr.h"
#include "mmb_ctx.h"
#include "mmb_adapter.h"
#include "mmb_miband.h"

static void adapter_scan_cb(void *pdata, struct mmb_adapter_scan_result_s * results, int size)
{
    int ret;
    MMB_CTX * this = pdata;
    struct mmb_adapter_scan_result_s * p = results;
    struct mmb_device_list_s * device = NULL;
    char addr[18];

    if (size <= 0)
        return;

    printf("[MMB][SCAN][CB] Find BLE Device: counts = %d\n", size);

    while (p) {

        ba2str(&p->addr, addr);
        printf("[MMB][SCAN][CB] ==> Name[%s], Addr[%s], Rssi[%d]\n", results->name, addr, results->rssi);

        if (strncmp(results->name, "MI", 2) == 0)
        {

            device = malloc(sizeof(struct mmb_device_list_s));

            strncpy(device->name, results->name, sizeof(device->name));
            bacpy(&device->addr, &results->addr);
            device->device_ctx = (void *) malloc(sizeof(struct mmb_miband_ctx_s));
            device->next = this->devices;

            // Init MIBAND
            printf("[MMB][SCAN][CB] Inital MIBAND: Name[%s], Addr[%s].\n", results->name, addr);
            if ((ret = mmb_miband_init(device->device_ctx, &results->addr, this->evhr)) < 0)
            {
                printf("[MMB][SCAN][CB] ERR: mmb_miband_init failed! ret[%d]", ret);
                free(device->device_ctx);
                free(device);
                return;
            }

            // Start MIBAND
            printf("[MMB][SCAN][CB] Start MIBAND: Name[%s], Addr[%s].\n", results->name, addr);
            if ((ret = mmb_miband_start(device->device_ctx, &this->adapter->addr)) < 0)
            {
                printf("[MMB][SCAN][CB] ERR: mmb_miband_start failed! ret = %d.\n", ret);
                free(device->device_ctx);
                free(device);
                return;
            }

            this->devices = device;

            return;
        }

        p = p->next;
    }

}

int mmb_service_init(MMB_CTX * this, bdaddr_t * addr)
{
    int ret = 0;

    // Default config
    memset(this, 0, sizeof(MMB_CTX));
    this->adapter = (MMB_ADAPTER *) malloc(sizeof(MMB_ADAPTER)); 
    this->devices = NULL;

    // Inital EVHR
    if ((ret = evhr_create(&this->evhr)) != EVHR_RTN_SUCCESS)
    {
        printf("[MMB][SERVICE] ERR: evhr_create failed! ret = %d\n", ret);
        return -1;
    }

    // Inital Adapter
    if ((ret = mmb_adapter_init(this->adapter, addr)) < 0)
    {
        printf("[MMB][SERVICE] ERR: evhr_create failed! ret = %d\n", ret);
        return -2;
    }

    printf("[MMB][SERVICE] initial.\n");

    return 0;
}

int mmb_service_start(MMB_CTX * this)
{
    int ret;
    struct mmb_device_list_s * device = NULL, * next_device = NULL;

    printf("[MMB][SERVICE] start.\n");

    while (1)
    {
        if ((ret = mmb_adapter_connect(this->adapter, this->evhr)) < 0)
        {
            printf("[MMB][SERVICE] ERR: mmb_adapter_connect failed! ret = %d.\n", ret);
            goto free_next_loop;
        }
        
        if ((ret = mmb_adapter_scan_start(this->adapter, this->evhr, adapter_scan_cb, this)) < 0)
        {
            printf("[MMB][SERVICE] ERR: mmb_adapter_scan_start failed! ret = %d.\n", ret);
            goto free_next_loop;
        }

        printf("[MMB][SERVICE] running.\n");
        evhr_dispatch(this->evhr);

free_next_loop:

        printf("[MMB][SERVICE] stop.\n");
        
        mmb_adapter_scan_stop(this->adapter);
        mmb_adapter_disconnect(this->adapter);

        device = this->devices;
        while (device)
        {
            mmb_miband_stop(device->device_ctx);
            free(device->device_ctx);
            next_device = device->next;
            free(device);
            device = next_device;
        }

        this->devices = NULL;
    }

    return 0;
}

