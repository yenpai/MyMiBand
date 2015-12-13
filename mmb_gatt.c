#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mmb_ctx.h"
#include "mmb_util.h"
#include "evhr.h"

extern MMB_CTX g_mmb_ctx;

static int do_gatt_listen_string_parsing(char * str)
{
    uint16_t hnd = 0x0000;
    char value[CMD_BUFFER_SIZE];

    // NOTIFICATION 
    if (!strncmp(str, RESPONSE_NOTIFICATION_STR, strlen(RESPONSE_NOTIFICATION_STR)))
    {
        sscanf(str + strlen(RESPONSE_NOTIFICATION_STR) + 1, "handle = 0x%hx value: %[^\n]s", &hnd, value);
        switch (hnd)
        {
            case MIBAND_CHAR_HND_SENSOR:
                printf("[GATT][NOTIFY][SENSOR] %s\n", value);
                break;
            default:
                printf("[GATT][NOTIFY][0x%04x] %s\n", hnd, value);
                break;
        }
        return 0;
    }

    return -1;
}

static int do_gatt_listen_buffer_parsing(struct mmb_gatt_s * gatt)
{
    size_t end = 0;
    size_t size = gatt->buf_size;
    char * buf  = gatt->buf;
    char * curr = buf;

    while (size > 0)
    {
        // Find line or string end
        for (end = 0; end < size; end++)
        {
            if (curr[end] == '\0' || curr[end] == '\n')
            {
                if (end == 0)
                    goto next_size_p;
                else
                    break;
            }
        }

        // No find line or string end
        if (end == size)
            break;

        // Start Parsing Command 
        curr[end] = '\0';
        if (do_gatt_listen_string_parsing(curr) != -1) {
            // TODO: update timer here.
            goto next_size_p;
        }

        // unknow command
        printf("[GATT][READ][UNKNOW] size = %ld, len = %ld, data = %s\n", size, end, curr);

next_size_p:

        curr += (end + 1);
        size -= (end + 1);
    }

    // shift buffer and size
    if (curr != buf) {
        gatt->buf_size = size;
        for ( end = 0 ; end < size ; end++)
            buf[end] = buf[end +(curr - buf)];
    }

    return size;
}

static void do_gatt_listen_stop(struct mmb_gatt_s * gatt)
{
    if (gatt == NULL)
        return;

    // close popen and event
    if (gatt->popen_fd)
    {
        evhr_event_del( ((MMB_CTX *) gatt->pdata)->evhr, fileno(gatt->popen_fd));
        pclose(gatt->popen_fd);
    }
    
    // stop timer and event
    if (gatt->timer_fd > 0)
    {
        evhr_event_del( ((MMB_CTX *) gatt->pdata)->evhr, gatt->timer_fd);
        evhr_event_stop_timer(gatt->timer_fd);
        close(gatt->timer_fd);
    }

    gatt->popen_fd   = NULL;
    gatt->timer_fd   = -1;
    gatt->buf_size   = 0;
    gatt->is_running = 0;

    // stop event handler
    if (gatt->pdata) {
        evhr_stop(((MMB_CTX *) gatt->pdata)->evhr);
    }

    printf("[GATT][STOP].\n");
}

static void callback_gatt_listen_timeout(int UNUSED(fd), void *pdata)
{
    struct mmb_gatt_s * gatt = (struct mmb_gatt_s *) pdata;
    printf("[GATT][TIMEOUT].\n");
    do_gatt_listen_stop(gatt);
}

static void callback_gatt_listen_read(int fd, void * pdata)
{
    int len = 0;
    char buf[CMD_BUFFER_SIZE];
    struct mmb_gatt_s * gatt = (struct mmb_gatt_s *) pdata;

    while (1)
    {
        len = read(fd, buf, CMD_BUFFER_SIZE);

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
        
        // check gatt buf size
        if (gatt->buf_size + len > sizeof(gatt->buf))
        {
            printf("[GATT][ERROR] Buffer Overflow!");
            goto disconnect;
        }

        // append data into gatt buf
        memcpy(gatt->buf + gatt->buf_size, buf, len);
        gatt->buf_size += len;

        // parsing gatt buf
        do_gatt_listen_buffer_parsing(gatt);
    }

    return;

disconnect:

    printf("[GATT][DISCONNECT].\n");
    do_gatt_listen_stop(gatt);

    return;
}

static void callback_gatt_listen_error(int UNUSED(fd), void * pdata)
{
    struct mmb_gatt_s * gatt = (struct mmb_gatt_s *) pdata;
    printf("[GATT][ERROR].\n");
    do_gatt_listen_stop(gatt);
}

int mmb_gatt_listen_start()
{
    int fd;
    char shell_cmd[CMD_BUFFER_SIZE];
    struct mmb_gatt_s * gatt = &g_mmb_ctx.gatt;
    
    gatt->popen_fd   = NULL;
    gatt->timer_fd   = -1;
    gatt->is_running = 0;
    gatt->buf_size   = 0;
    gatt->pdata      = (void *) &g_mmb_ctx;

    // Start Listen
    snprintf(shell_cmd, CMD_BUFFER_SIZE, "%s -i %s -b %s --char-read -a 0x%04x --listen;", 
            CMD_GATTTOOL_PATH, g_mmb_ctx.hci_dev, g_mmb_ctx.miband_mac, MIBAND_CHAR_HND_USERINFO);
    printf("[GATT][CMD] %s\n", shell_cmd);

    // popen
    if ((gatt->popen_fd = popen(shell_cmd, "r")) == NULL)
    {
        printf("[GATT][ERROR] popen failed!\n");
        return -1;
    }              

    fd = fileno(gatt->popen_fd);
    socket_setting_non_blocking(fd);

    // Add popen into event handler
    evhr_event_add_socket(
            g_mmb_ctx.evhr, fd, gatt,
            callback_gatt_listen_read, callback_gatt_listen_error);
    
    // Create timerfd
    gatt->timer_fd = evhr_event_create_timer();
    if (gatt->timer_fd < 0)
    {
        printf("[GATT][ERROR] timer create failed!\n");
        do_gatt_listen_stop(gatt);
        return -2;
    }

    // Add timer into event handler
    if (evhr_event_add_timer_periodic(
            g_mmb_ctx.evhr, gatt->timer_fd,
            MMB_GATT_LISTEN_TIMEOUT_SEC, 10, /* MMB_GATT_LISTEN_TIMEOUT_SEC + 10 nsec timeout */
            gatt, callback_gatt_listen_timeout) != EVHR_RTN_SUCCESS)
    {
        // TODO:
        printf("[GATT][ERROR] timer event binding failed!\n");
        do_gatt_listen_stop(gatt);
        return -3;
    }

    g_mmb_ctx.gatt.is_running = 1;

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
    uint8_t value[2] = {0x00, 0x00};

    // Disable Sensor Data
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR, value, 2);
    if (ret != 0)
        return ret;

    // Disable Sensor Notify
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
    if (ret != 0)
        return ret;

    return 0;
}

int mmb_gatt_send_sensor_notify_enable()
{
    int ret = 0;
    uint8_t value[2] = {0x02, 0x00};

    // Enable Sensor Data
    ret = mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR, value, 2);
    if (ret != 0)
        return ret;

    // Endbale Sensor Notify
    return mmb_gatt_send_char_hnd_write_req(MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
}
