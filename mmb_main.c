#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>  

#include "mmb_user_info.h"

#define CMD_BUFFER_SIZE     512
#define CMD_GATTTOOL_PATH   "/usr/bin/gatttool"

#define RESPONSE_NOTIFICATION_STR "Notification"

#define MIBAND_CHAR_HND_USERINFO        0x0019
#define MIBAND_CHAR_HND_SENSOR_DATA     0x0031
#define MIBAND_CHAR_HND_SENSOR_NOTIFY   0x0032

extern FILE *popen(const char *command, const char *type);
extern int pclose(FILE *stream);

static char g_mmb_hci_dev[6];
static char g_mmb_mac[18];
static struct mmb_user_info_s g_mmb_user_info;

static int bytes_to_hex_str(char * out, uint8_t * in, size_t size)
{
    size_t i;
    char *p = out;
    for (i=0;i<size;i++)
        p += sprintf(p, "%02x", in[i]);
    *p = '\0';
    return (p - out);
}

static int mmb_gatt_send_char_hnd_write_req(uint8_t hnd, uint8_t * data, size_t size)
{
    char cmd[CMD_BUFFER_SIZE];
    char buf[CMD_BUFFER_SIZE];

    if ((size * 2) > (CMD_BUFFER_SIZE - 1))
        return -1;

    bytes_to_hex_str(buf, data, size);

    snprintf(cmd, CMD_BUFFER_SIZE, 
            "%s -i %s -b %s --char-write-req -a 0x%04x -n %s;", 
            CMD_GATTTOOL_PATH, g_mmb_hci_dev, g_mmb_mac, hnd, buf);
    printf("[CMD] %s\n", cmd);

    return system(cmd);
} 

int mmb_send_user_info(struct mmb_user_info_s * user_info)
{
    return mmb_gatt_send_char_hnd_write_req(
            MIBAND_CHAR_HND_USERINFO, 
            user_info->data, 
            sizeof(user_info->data));
}

int mmb_enable_sensor_notify()
{
    int ret = 0;
    uint8_t value[2];

    // Disable
    value[0] = 0x00;
    value[1] = 0x00;
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_DATA, value, 2);
    if (ret != 0)
        return ret;

    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
    if (ret != 0)
        return ret;

    // Enable
    value[0] = 0x02;
    value[1] = 0x00;
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_DATA, value, 2);
    if (ret != 0)
        return ret;

    return mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
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
    mmb_send_user_info(&g_mmb_user_info);

    // Send Sense Data Notification Disable/Enable
    mmb_enable_sensor_notify();

    // Start Listen
    snprintf(buf, CMD_BUFFER_SIZE, "%s -i %s -b %s --char-read -a 0x%04x --listen;", 
            CMD_GATTTOOL_PATH, g_mmb_hci_dev, g_mmb_mac, MIBAND_CHAR_HND_USERINFO);
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
    strncpy(g_mmb_hci_dev, "hci0", sizeof(g_mmb_hci_dev));
    strncpy(g_mmb_mac, "88:0F:10:2A:5F:08", sizeof(g_mmb_mac));

    // Default UserInfo
    memset(&g_mmb_user_info, 0, sizeof(struct mmb_user_info_s));
    g_mmb_user_info.uid       = 19820610;
    g_mmb_user_info.gender    = 1;
    g_mmb_user_info.age       = 32;
    g_mmb_user_info.height    = 175;
    g_mmb_user_info.weight    = 60;
    g_mmb_user_info.type      = 0;
    strncpy((char *)g_mmb_user_info.alias, "RobinMI", sizeof(g_mmb_user_info.alias));
    
    while ((opt = getopt_long(argc, argv, optarg_so, optarg_lo, NULL)) != -1)
    {
        switch(opt)
        {
            case 'i':
                strncpy(g_mmb_hci_dev, optarg, sizeof(g_mmb_hci_dev));
                break;
            case 'b':
                strncpy((char *)g_mmb_mac, optarg, sizeof(g_mmb_mac));
                break;
            case 'h':
                printf("Usage:\n\n");
                exit(0);
        }
    }

    // Generate User Info Code
    mmb_user_info_gen_code(&g_mmb_user_info, atol(g_mmb_mac + 15) & 0xFF);

    if ((ret = mmb_mainloop()) < 0)
    {
        printf("mmb_connect failed! ret[%d]", ret);
        exit(1);
    }

    return 0;
}
