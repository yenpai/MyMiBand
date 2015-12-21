#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>  

#include "mmb_ctx.h"
#include "evhr.h"

/* mmb_miband.c */ 
extern int mmb_miband_start(MMB_CTX *);
extern int mmb_miband_stop(MMB_CTX *);

int mmb_mainloop(MMB_CTX * mmb)
{
    while (1)
    {
        printf("[MMB][MAINLOOP] connect.\n");

        if (mmb_miband_start(mmb) < 0)
        {
            printf("[MMB][MAINLOOP][ERR] mmb_miband_start failed!\n");
            goto free_next_loop;
        }

        printf("[MMB][MAINLOOP] start.\n");
        evhr_dispatch(mmb->evhr);

free_next_loop:

        printf("[MMB][MAINLOOP] exit.\n");
        mmb_miband_stop(mmb);
    }

    return 0;
}

static char *        optarg_so = "i:b:h";
static struct option optarg_lo[] = {
    { "Interface",  1, NULL, 'i' },
    { "MiBand MAC", 1, NULL, 'b' },
    { "Help", 0, NULL, 'h' },
    { 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
    int ret, opt;
    MMB_CTX mmb;

    // Default config
    memset(&mmb, 0, sizeof(MMB_CTX));
    bacpy(&mmb.addr, BDADDR_ANY);
    //str2ba("88:0F:10:2A:5F:08", &mmb.addr);

    // Default MiBand data
    str2ba("88:0F:10:2A:5F:08", &mmb.data.addr);
    mmb.data.user.uid       = 19820610;
    mmb.data.user.gender    = 1;
    mmb.data.user.age       = 32;
    mmb.data.user.height    = 175;
    mmb.data.user.weight    = 60;
    mmb.data.user.type      = 0;
    strcpy((char *)mmb.data.user.alias, "RobinMI");

    // Got Arg
    while ((opt = getopt_long(argc, argv, optarg_so, optarg_lo, NULL)) != -1)
    {
        switch(opt)
        {
            case 'i':
                if (!strncasecmp(optarg, "hci", 3))
                    hci_devba(atoi(optarg + 3), &mmb.addr);
                else
                    str2ba(optarg, &mmb.addr);
                break;
            case 'b':
                str2ba(optarg, &mmb.data.addr);
                break;
            case 'h':
                printf("Usage:\n\n");
                exit(0);
        }
    }

    // Inital EVHR
    if ((ret = evhr_create(&mmb.evhr)) != EVHR_RTN_SUCCESS)
    {
        printf("evhr_create failed! ret = %d\n", ret);
        exit(1);
    }
    
    // Start Main Loop
    if ((ret = mmb_mainloop(&mmb)) < 0)
    {
        printf("mmb_connect failed! ret[%d]", ret);
        exit(2);
    }

    return 0;
}

