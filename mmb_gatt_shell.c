#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * unused attribute macros
 * example:
 *   void foo(int UNUSED(bar)) { ... }
 *   static void UNUSED_FUNCTION(foo)(int bar) { ... }
 */
#ifdef __GNUC__
    #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
    #define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
    #define UNUSED(x) UNUSED_ ## x
    #define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

#include "mmb_ctx.h"
#include "mmb_util.h"
#include "evhr.h"

extern MMB_CTX g_mmb_ctx;

static void do_gatt_listen_disconnect()
{
    if (g_mmb_ctx.gatt_shell_running != 0) {
        evhr_event_del( g_mmb_ctx.evhr, g_mmb_ctx.gatt_shell_fd);
        pclose(g_mmb_ctx.gatt_shell_file_fd);
        g_mmb_ctx.gatt_shell_running = 0;
    }
}

static int do_gatt_listen_buffer_parsing(char *buf, int size)
{
    int i, old_size = size;
    char * p = buf;
    unsigned short hnd = 0x0000;
    char value[CMD_BUFFER_SIZE];

    while (size > 0)
    {
        // Find line or string end
        for (i = 0; i < size; i++)
        {
            if (p[i] == '\0' || p[i] == '\n')
            {
                if (i == 0)
                    goto next_size_p;
                else
                    break;
            }
        }

        // No find line or string end
        if (i == size)
            break;

        // Start Parsing Command 
        if (!strncmp(p, RESPONSE_NOTIFICATION_STR, strlen(RESPONSE_NOTIFICATION_STR)))
        {
            sscanf(buf, "Notification handle = 0x%hx value: %[^\n]s", &hnd, value);
            printf("[GATT][NOTIFY][0x%04x] %s\n", hnd, value);
            goto next_size_p;
        }

        // unknow command
        p[i] = '\0';
        printf("[GATT][READ][UNKNOW] size = %d, len = %d, data = %s\n", size, i, p);

next_size_p:

        p += (i + 1);
        size -= (i + 1);
    }

    for (i=0;i<size;i++)
        buf[i] = buf[i+(old_size-size)];

    return size;
}

static void callback_gatt_listen_read(int fd, void * UNUSED(pdata))
{
    int len = 0, size = 0;
    char buf[CMD_BUFFER_SIZE];

    while (1)
    {
        len = read(fd, buf + size, sizeof(buf) - size);

        if (len == 0)
        {
            goto disconnect;
        }
        else if (len < 0)
        {
            // No data in socket buffer
            if (errno == EAGAIN)
                break;
            goto disconnect;
        }
        
        size += len;
        size = do_gatt_listen_buffer_parsing(buf, size);
    }

    return;

disconnect:

    printf("[GATT][Disconnect].\n\n");
    do_gatt_listen_disconnect();

    return;
}

static void callback_gatt_listen_error(int UNUSED(fd), void * UNUSED(pdata))
{
    printf("[GATT][ERROR].\n");
    do_gatt_listen_disconnect();
}

int mmb_gatt_listen_start()
{
    char buf[CMD_BUFFER_SIZE];
    
    memset(buf, 0, sizeof(buf));

    // Start Listen
    snprintf(buf, CMD_BUFFER_SIZE, "%s -i %s -b %s --char-read -a 0x%04x --listen;", 
            CMD_GATTTOOL_PATH, g_mmb_ctx.hci_dev, g_mmb_ctx.miband_mac, MIBAND_CHAR_HND_USERINFO);
    printf("[GATT][CMD] %s\n", buf);

    // popen
    if ((g_mmb_ctx.gatt_shell_file_fd = popen(buf, "r")) == NULL)
    {
        printf("popen failed!\n");
        return -1;
    }              

    g_mmb_ctx.gatt_shell_fd = fileno(g_mmb_ctx.gatt_shell_file_fd);
    socket_setting_non_blocking(g_mmb_ctx.gatt_shell_fd);

    // Add fd into event handler
    evhr_event_add_socket(
            g_mmb_ctx.evhr, g_mmb_ctx.gatt_shell_fd, NULL,
            callback_gatt_listen_read, callback_gatt_listen_error);
    
    g_mmb_ctx.gatt_shell_running= 1;

    return 0;
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
            CMD_GATTTOOL_PATH, g_mmb_ctx.hci_dev, g_mmb_ctx.miband_mac, hnd, buf);
    printf("[GATT][CMD] %s\n", cmd);

    return system(cmd);
} 

int mmb_gatt_send_user_info()
{
    uint8_t byte;

    // Generate User Info Code, CRC8(0~18 Byte) ^ MAC[last byte]
    byte = atol(g_mmb_ctx.miband_mac + strlen(g_mmb_ctx.miband_mac) - 2) & 0xFF;
    g_mmb_ctx.user_info.code = crc8(0x00, g_mmb_ctx.user_info.data, sizeof(g_mmb_ctx.user_info.data) - 1) ^ byte;

    return mmb_gatt_send_char_hnd_write_req(
            MIBAND_CHAR_HND_USERINFO, 
            g_mmb_ctx.user_info.data, 
            sizeof(g_mmb_ctx.user_info.data));
}

int mmb_gatt_send_sensor_notify_disable()
{
    int ret = 0;
    uint8_t value[2];

    // Disable Sensor Data
    value[0] = 0x00;
    value[1] = 0x00;
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_DATA, value, 2);
    if (ret != 0)
        return ret;

    // Disable Sensor Notify
    value[0] = 0x00;
    value[1] = 0x00;
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
    if (ret != 0)
        return ret;

    return 0;
}

int mmb_gatt_send_sensor_notify_enable()
{
    int ret = 0;
    uint8_t value[2];

    // Enable Sensor Data
    value[0] = 0x02;
    value[1] = 0x00;
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_DATA, value, 2);
    if (ret != 0)
        return ret;

    // Endbale Sensor Notify
    value[0] = 0x02;
    value[1] = 0x00;
    return mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
}
