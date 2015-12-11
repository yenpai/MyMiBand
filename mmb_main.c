#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>  

#include "mmb_util.h"
#include "mmb_ctx.h"

static MMB_CTX g_mmb_ctx;

static int mmb_send_gatt_char_hnd_write_req(uint8_t hnd, uint8_t * data, size_t size)
{
    char cmd[CMD_BUFFER_SIZE];
    char buf[CMD_BUFFER_SIZE];

    if ((size * 2) > (CMD_BUFFER_SIZE - 1))
        return -1;

    bytes_to_hex_str(buf, data, size);

    snprintf(cmd, CMD_BUFFER_SIZE, 
            "%s -i %s -b %s --char-write-req -a 0x%04x -n %s;", 
            CMD_GATTTOOL_PATH, g_mmb_ctx.hci_dev, g_mmb_ctx.miband_mac, hnd, buf);
    printf("[CMD] %s\n", cmd);

    return system(cmd);
} 

int mmb_send_user_info()
{
    uint8_t byte;

    // Generate User Info Code, CRC8(0~18 Byte) ^ MAC[last byte]
    byte = atol(g_mmb_ctx.miband_mac + strlen(g_mmb_ctx.miband_mac) - 2) & 0xFF;
    g_mmb_ctx.user_info.code = crc8(0x00, g_mmb_ctx.user_info.data, sizeof(g_mmb_ctx.user_info.data) - 1) ^ byte;

    return mmb_send_gatt_char_hnd_write_req(
            MIBAND_CHAR_HND_USERINFO, 
            g_mmb_ctx.user_info.data, 
            sizeof(g_mmb_ctx.user_info.data));
}

int mmb_send_sensor_notify_disable()
{
    int ret = 0;
    uint8_t value[2];

    // Disable Sensor Data
    value[0] = 0x00;
    value[1] = 0x00;
    ret = mmb_send_gatt_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_DATA, value, 2);
    if (ret != 0)
        return ret;

    // Disable Sensor Notify
    value[0] = 0x00;
    value[1] = 0x00;
    ret = mmb_send_gatt_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
    if (ret != 0)
        return ret;

    return 0;
}

int mmb_send_sensor_notify_enable()
{
    int ret = 0;
    uint8_t value[2];

    // Enable Sensor Data
    value[0] = 0x02;
    value[1] = 0x00;
    ret = mmb_send_gatt_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_DATA, value, 2);
    if (ret != 0)
        return ret;

    // Endbale Sensor Notify
    value[0] = 0x02;
    value[1] = 0x00;
    return mmb_send_gatt_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
}

static int mmb_gatt_listen_start()
{
    return 0;
}

int mmb_mainloop()
{
    FILE *fd = NULL;
    char buf[CMD_BUFFER_SIZE];

    unsigned short hnd = 0x0000;
    char value[CMD_BUFFER_SIZE];
    
    memset(buf, 0, sizeof(buf));

    // Send USER_INFO
    mmb_send_user_info();

    // Send Sense Data Notification Disable/Enable
    mmb_send_sensor_notify_disable();
    mmb_send_sensor_notify_enable();

    // Start Listen
    snprintf(buf, CMD_BUFFER_SIZE, "%s -i %s -b %s --char-read -a 0x%04x --listen;", 
            CMD_GATTTOOL_PATH, g_mmb_ctx.hci_dev, g_mmb_ctx.miband_mac, MIBAND_CHAR_HND_USERINFO);
    printf("[CMD] %s\n", buf);

    // popen
    if ((fd = popen(buf, "r")) == NULL)
    {
        printf("popen failed!\n");
        return -1;
    }              

    // Read
    while (fgets(buf, CMD_BUFFER_SIZE, fd))
    {
        if (!strncmp(buf, RESPONSE_NOTIFICATION_STR, strlen(RESPONSE_NOTIFICATION_STR)))
        {
            sscanf(buf, "Notification handle = 0x%hx value: %[^\n]s", &hnd, value);
            printf("SENSOR_NOTIFY [0x%04x] %s\n", hnd, value);
        }
    }

    pclose(fd);

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
    
    while ((opt = getopt_long(argc, argv, optarg_so, optarg_lo, NULL)) != -1)
    {
        switch(opt)
        {
            case 'i':
                strncpy(g_mmb_ctx.hci_dev, optarg, sizeof(g_mmb_ctx.hci_dev));
                break;
            case 'b':
                strncpy((char *)g_mmb_ctx.miband_mac, optarg, sizeof(g_mmb_ctx.miband_mac));
                break;
            case 'h':
                printf("Usage:\n\n");
                exit(0);
        }
    }

    if ((ret = mmb_mainloop()) < 0)
    {
        printf("mmb_connect failed! ret[%d]", ret);
        exit(1);
    }

    return 0;
}

