#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>  

#include "mmb_ctx.h"
#include "evhr.h"

/* mmb_service.c */
extern int mmb_service_init(MMB_CTX *);
extern int mmb_service_start(MMB_CTX * mmb);

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

    if ((ret = mmb_service_init(&mmb)) < 0)
    {
        printf("mmb_service_init failed! ret[%d]", ret);
        return -1;
    }

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

    // Start Service
    if ((ret = mmb_service_start(&mmb)) < 0)
    {
        printf("mmb_service_start failed! ret[%d]", ret);
        return -2;
    }

    return 0;
}

