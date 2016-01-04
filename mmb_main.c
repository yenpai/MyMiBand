#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>  

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "mmb_ctx.h"

static char *        optarg_so = "i:h";
static struct option optarg_lo[] = {
    { "Interface",  1, NULL, 'i' },
    { "Help", 0, NULL, 'h' },
    { 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
    int ret, opt;
    bdaddr_t adapter_addr;

    // Default mac addr
    bacpy(&adapter_addr, BDADDR_ANY);

    // Got Arg
    while ((opt = getopt_long(argc, argv, optarg_so, optarg_lo, NULL)) != -1)
    {
        switch(opt)
        {
            case 'i':
                if (!strncmp(optarg, "hci", 3))
                    hci_devba(atoi(optarg + 3), &adapter_addr);
                else
                    str2ba(optarg, &adapter_addr);
                break;
            case 'h':
                printf("Usage:\n\n");
                exit(0);
        }
    }

    // Init Service
    if ((ret = mmb_service_init(&adapter_addr)) < 0)
    {
        printf("mmb_service_init failed! ret[%d]", ret);
        return -1;
    }

    // Start Service
    if ((ret = mmb_service_start()) < 0)
    {
        printf("mmb_service_start failed! ret[%d]", ret);
        return -2;
    }

    return 0;
}

