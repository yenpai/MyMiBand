#include "stdlib.h"
#include "evhr.h"
#include "mmb_ctx.h"

#include "mmb_miband.h"

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
    }

    return 0;
}

