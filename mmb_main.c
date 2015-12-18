#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>  

#include "mmb_util.h"
#include "mmb_ctx.h"
#include "evhr.h"

/* mmb_handle.c */ 
extern int mmb_handle_send_auth(MMB_CTX *);
extern int mmb_handle_send_sensor_notify_disable(MMB_CTX *);
extern int mmb_handle_send_sensor_notify_enable(MMB_CTX *);
extern int mmb_gatt_listen_start(MMB_CTX *); 

int mmb_mainloop(MMB_CTX * mmb)
{
    while(1)
    {
        // Send Auth
        mmb_handle_send_auth(mmb);

        // Send Sense Data Notification Disable/Enable
        mmb_handle_send_sensor_notify_disable(mmb);
        mmb_handle_send_sensor_notify_enable(mmb);

        // Start Listen
        mmb_gatt_listen_start(mmb);

        // Start Event Handler Dispatch (blocking)
        evhr_dispatch(mmb->evhr);

        printf("[MMB][MAINLOOP] restart.\n");
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
    strcpy(mmb.hci_dev,      "hci0");
    strcpy(mmb.data.miband_mac,   "88:0F:10:2A:5F:08");

    // Default UserInfo
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
                strncpy(mmb.hci_dev, optarg, sizeof(mmb.hci_dev));
                break;
            case 'b':
                strncpy(mmb.data.miband_mac, optarg, sizeof(mmb.data.miband_mac));
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

