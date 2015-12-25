#include <unistd.h>
#include <stdlib.h>

#include "evhr.h"
#include "mmb_ctx.h"
#include "mmb_adapter.h"
#include "mmb_miband.h"

int mmb_service_init(MMB_CTX * this, bdaddr_t * addr)
{
    int ret = 0;

    // Default config
    memset(this, 0, sizeof(MMB_CTX));
    this->miband  = (MMB_MIBAND *) malloc(sizeof(MMB_MIBAND));
    this->adapter = (MMB_ADAPTER *) malloc(sizeof(MMB_ADAPTER)); 

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

    // Init MIBAND
    // TODO: NULL ===> MAC
    if ((ret = mmb_miband_init(this->miband, BDADDR_ANY, this->evhr)) < 0)
    {
        printf("mmb_miband_init failed! ret[%d]", ret);
        return -3;
    }

    printf("[MMB][SERVICE] initial.\n");

    return 0;
}

int mmb_service_start(MMB_CTX * this)
{
    int ret;

    printf("[MMB][SERVICE] start.\n");

    while (1)
    {
        if ((ret = mmb_adapter_connect(this->adapter, this->evhr)) < 0)
        {
            printf("[MMB][SERVICE][ERR] mmb_adapter_connect failed! ret = %d.\n", ret);
            goto free_next_loop;
        }
        
        if ((ret = mmb_adapter_scan_start(this->adapter, NULL)) < 0)
        {
            printf("[MMB][SERVICE][ERR] mmb_adapter_scan_start failed! ret = %d.\n", ret);
            goto free_next_loop;
        }

        if ((ret = mmb_miband_start(this->miband, &this->adapter->addr)) < 0)
        {
            printf("[MMB][SERVICE][ERR] mmb_miband_start failed! ret = %d.\n", ret);
            goto free_next_loop;
        }

        printf("[MMB][SERVICE] running.\n");
        evhr_dispatch(this->evhr);

free_next_loop:

        printf("[MMB][SERVICE] stop.\n");
        mmb_miband_stop(this->miband);
        mmb_adapter_disconnect(this->adapter);
        mmb_adapter_scan_stop(this->adapter);
    }

    return 0;
}

