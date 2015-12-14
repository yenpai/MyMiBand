#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>  

#include "mmb_util.h"
#include "mmb_ctx.h"
#include "evhr.h"

MMB_CTX g_mmb_ctx;

/* mmb_gatt.c */ 
extern int mmb_gatt_send_user_info(struct mmb_user_info_s * user, char * miband_mac);
extern int mmb_gatt_send_sensor_notify_disable();
extern int mmb_gatt_send_sensor_notify_enable();
extern int mmb_gatt_listen_start(); 

int mmb_mainloop()
{
    while(1)
    {
        // Send USER_INFO
        mmb_gatt_send_user_info(&g_mmb_ctx.user_info, g_mmb_ctx.miband_mac);

        // Send Sense Data Notification Disable/Enable
        mmb_gatt_send_sensor_notify_disable();
        mmb_gatt_send_sensor_notify_enable();

        // Start Listen
        mmb_gatt_listen_start();

        // Start Event Handler Dispatch (blocking)
        evhr_dispatch(g_mmb_ctx.evhr);

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

    // Default config
    memset(&g_mmb_ctx, 0, sizeof(MMB_CTX));
    strcpy(g_mmb_ctx.hci_dev,      "hci0");
    strcpy(g_mmb_ctx.miband_mac,   "88:0F:10:2A:5F:08");

    // Default UserInfo
    g_mmb_ctx.user_info.uid       = 19820610;
    g_mmb_ctx.user_info.gender    = 1;
    g_mmb_ctx.user_info.age       = 32;
    g_mmb_ctx.user_info.height    = 175;
    g_mmb_ctx.user_info.weight    = 60;
    g_mmb_ctx.user_info.type      = 0;
    strcpy((char *)g_mmb_ctx.user_info.alias, "RobinMI");

    // Got Arg
    while ((opt = getopt_long(argc, argv, optarg_so, optarg_lo, NULL)) != -1)
    {
        switch(opt)
        {
            case 'i':
                strncpy(g_mmb_ctx.hci_dev, optarg, sizeof(g_mmb_ctx.hci_dev));
                break;
            case 'b':
                strncpy(g_mmb_ctx.miband_mac, optarg, sizeof(g_mmb_ctx.miband_mac));
                break;
            case 'h':
                printf("Usage:\n\n");
                exit(0);
        }
    }

    // Inital EVHR
    if ((ret = evhr_create(&g_mmb_ctx.evhr)) != EVHR_RTN_SUCCESS)
    {
        printf("evhr_create failed! ret = %d\n", ret);
        exit(1);
    }
    
    // Start Main Loop
    if ((ret = mmb_mainloop()) < 0)
    {
        printf("mmb_connect failed! ret[%d]", ret);
        exit(2);
    }

    return 0;
}

