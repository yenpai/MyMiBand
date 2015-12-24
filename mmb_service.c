#include "stdlib.h"
#include "evhr.h"
#include "mmb_ctx.h"

#include "mmb_miband.h"

int mmb_service_init(MMB_CTX * this)
{
    int ret = 0;
    bdaddr_t dest;

    // Default config
    memset(this, 0, sizeof(MMB_CTX));
    bacpy(&this->addr, BDADDR_ANY);
    this->miband = (MMB_MIBAND *) malloc(sizeof(MMB_MIBAND));

    // Inital EVHR
    if ((ret = evhr_create(&this->evhr)) != EVHR_RTN_SUCCESS)
    {
        printf("[MMB][SERVICE] ERR: evhr_create failed! ret = %d\n", ret);
        return -1;
    }

    // Inital MIBAND
    str2ba("88:0F:10:2A:5F:08", &dest);
    if ((ret = mmb_miband_init(this->miband, &dest, this->evhr)) < 0)
    {
        printf("[MMB][SERVICE] ERR: mmb_miband_init failed! ret = %d\n", ret);
        evhr_release(this->evhr);
        return -2;
    }

    return 0;
}

int mmb_service_start(MMB_CTX * this)
{
    while (1)
    {
        if (mmb_miband_start(this->miband, &this->addr) < 0)
        {
            printf("[MMB][SERVICE][ERR] mmb_miband_start failed!\n");
            goto free_next_loop;
        }

        printf("[MMB][SERVICE] start.\n");
        evhr_dispatch(this->evhr);

free_next_loop:

        printf("[MMB][SERVICE] exit.\n");
        mmb_miband_stop(this->miband);
    }

    return 0;
}

