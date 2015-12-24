#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>  

#include "mmb_ctx.h"

/* mmb_service.c */
extern int mmb_service_init(MMB_CTX * this, bdaddr_t * ble_adapter_addr);
extern int mmb_service_init_miband(MMB_CTX * this, bdaddr_t * dest);
extern int mmb_service_start(MMB_CTX * this);

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
    bdaddr_t adapter_addr;
    bdaddr_t miband_addr;

    // Default mac addr
    bacpy(&adapter_addr, BDADDR_ANY);
    str2ba("88:0F:10:2A:5F:08", &miband_addr);

    // Got Arg
    while ((opt = getopt_long(argc, argv, optarg_so, optarg_lo, NULL)) != -1)
    {
        switch(opt)
        {
            case 'i':
                if (!strncasecmp(optarg, "hci", 3))
                    hci_devba(atoi(optarg + 3), &adapter_addr);
                else
                    str2ba(optarg, &adapter_addr);
                break;
            case 'b':
                str2ba(optarg, &miband_addr);
                break;
            case 'h':
                printf("Usage:\n\n");
                exit(0);
        }
    }

    // Init Service
    if ((ret = mmb_service_init(&mmb, &adapter_addr)) < 0)
    {
        printf("mmb_service_init failed! ret[%d]", ret);
        return -1;
    }

    // Init MIBAND
    // TODO: Need implement mutile devices support
    if ((ret = mmb_service_init_miband(&mmb, &miband_addr)) < 0)
    {
        printf("mmb_service_init_miband failed! ret[%d]", ret);
        return -1;
    }

    // Start Service
    if ((ret = mmb_service_start(&mmb)) < 0)
    {
        printf("mmb_service_start failed! ret[%d]", ret);
        return -2;
    }

    return 0;
}

