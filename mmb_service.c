
#include "evhr.h"
#include "mmb_ctx.h"

/* mmb_miband.c */ 
extern int mmb_miband_start(MMB_CTX *);
extern int mmb_miband_stop(MMB_CTX *);

int mmb_service_init(MMB_CTX * mmb)
{
    int ret = 0;

    // Default config
    memset(mmb, 0, sizeof(MMB_CTX));
    bacpy(&mmb->addr, BDADDR_ANY);

    // Default MiBand data
    str2ba("88:0F:10:2A:5F:08", &mmb->data.addr);
    mmb->data.user.uid       = 19820610;
    mmb->data.user.gender    = 1;
    mmb->data.user.age       = 32;
    mmb->data.user.height    = 175;
    mmb->data.user.weight    = 60;
    mmb->data.user.type      = 0;
    strcpy((char *)mmb->data.user.alias, "RobinMI");

    // Inital EVHR
    if ((ret = evhr_create(&mmb->evhr)) != EVHR_RTN_SUCCESS)
    {
        printf("evhr_create failed! ret = %d\n", ret);
        return -1;
    }

    return 0;
}

int mmb_service_start(MMB_CTX * mmb)
{
    while (1)
    {
        if (mmb_miband_start(mmb) < 0)
        {
            printf("[MMB][SERVICE][ERR] mmb_miband_start failed!\n");
            goto free_next_loop;
        }

        printf("[MMB][SERVICE] start.\n");
        evhr_dispatch(mmb->evhr);

free_next_loop:

        printf("[MMB][SERVICE] exit.\n");
        mmb_miband_stop(mmb);
    }

    return 0;
}

