#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mmb_ctx.h"
#include "mmb_util.h"
#include "evhr.h"

extern MMB_CTX g_mmb_ctx;

static size_t hex_str_to_byte(uint8_t *arr, size_t max, char *str, const char * split)
{
    char *s = NULL;
    size_t i = 0;

    s = strtok(str, split);

    while(s != NULL)
    {
        arr[i] = 0xFF & strtoul(s, NULL, 16);
        s = strtok(NULL, split); 
        
        if (++i > max)
            break;
    }

    return i;
}

static int do_gatt_listen_string_parsing(char * str)
{
    uint16_t hnd = 0x0000;
    char tmp[CMD_BUFFER_SIZE];
    uint8_t buf[256];
    size_t buf_len = 0;
    
    // NOTIFICATION 
    if (!strncmp(str, RESPONSE_NOTIFICATION_STR, strlen(RESPONSE_NOTIFICATION_STR)))
    {
        sscanf(str + strlen(RESPONSE_NOTIFICATION_STR) + 1, "handle = 0x%hx value: %[^\n]s", &hnd, tmp);
        switch (hnd)
        {
            case MIBAND_CHAR_HND_SENSOR:
                buf_len = hex_str_to_byte( buf, sizeof(buf), tmp, " ");
                if (buf_len >= 8)
                {
                    // sensor data
                    printf("[GATT][NOTIFY][SENSOR] SEQ(%u) X(%u) Y(%u) Z(%u)\n", 
                            buf[0] | buf[1]<<8,
                            buf[2] | buf[3]<<8,
                            buf[4] | buf[5]<<8,
                            buf[6] | buf[7]<<8);
                }
                else
                {
                    printf("[GATT][NOTIFY][SENSOR] Data Len Not enought! [%s]\n", tmp);
                }

                break;
            default:
                printf("[GATT][NOTIFY][0x%04x] %s\n", hnd, tmp);
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
        if (do_gatt_listen_string_parsing(curr) != -1)
            goto next_size_p;

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
    
    printf("[GATT][STOP].\n");
    system("killall gatttool");

    // close popen and event
    if (gatt->popen_fd != NULL)
        pclose(gatt->popen_fd);
    
    if (gatt->proc_ev)
        evhr_event_del( ((MMB_CTX *) gatt->pdata)->evhr, gatt->proc_ev);
    
    // stop timer and event
    if (gatt->time_ev)
    {
        evhr_event_stop_timer(gatt->time_ev->fd);
        close(gatt->time_ev->fd);
        evhr_event_del( ((MMB_CTX *) gatt->pdata)->evhr, gatt->time_ev);
    }

    gatt->popen_fd      = NULL;
    gatt->proc_ev       = NULL;
    gatt->time_ev       = NULL;
    gatt->buf_size      = 0;
    gatt->is_running    = 0;

    // stop event handler
    if (gatt->pdata)
        evhr_stop(((MMB_CTX *) gatt->pdata)->evhr);

}

static void callback_gatt_listen_timeout(void *pdata)
{
    EVHR_EVENT * event = (EVHR_EVENT *) pdata;
    struct mmb_gatt_s * gatt = (struct mmb_gatt_s *) event->pdata;
    printf("[GATT][TIMEOUT].\n");
    do_gatt_listen_stop(gatt);
}

static void callback_gatt_listen_read(void * pdata)
{
    int len = 0;
    char buf[CMD_BUFFER_SIZE];
    EVHR_EVENT * event = (EVHR_EVENT *) pdata;
    struct mmb_gatt_s * gatt = (struct mmb_gatt_s *) event->pdata;

    while (1)
    {
        len = read(event->fd, buf, CMD_BUFFER_SIZE);

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

        // Update timer
        evhr_event_set_timer(
                gatt->time_ev->fd, MMB_GATT_LISTEN_TIMEOUT_SEC, 10, 0);
    }

    return;

disconnect:

    printf("[GATT][DISCONNECT].\n");
    do_gatt_listen_stop(gatt);

    return;
}

static void callback_gatt_listen_error(void * pdata)
{
    EVHR_EVENT * event = (EVHR_EVENT *) pdata;
    struct mmb_gatt_s * gatt = (struct mmb_gatt_s *) event->pdata;

    printf("[GATT][ERROR].\n");
    do_gatt_listen_stop(gatt);
}

int mmb_gatt_listen_start()
{
    int fd = -1;
    char shell_cmd[CMD_BUFFER_SIZE];
    struct mmb_gatt_s * gatt = &g_mmb_ctx.gatt;
    
    gatt->popen_fd      = NULL;
    gatt->proc_ev       = NULL;
    gatt->time_ev       = NULL;
    gatt->buf_size      = 0;
    gatt->is_running    = 0;
    gatt->pdata         = (void *) &g_mmb_ctx;

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
    gatt->proc_ev = evhr_event_add_socket(
            g_mmb_ctx.evhr, fd, gatt,
            callback_gatt_listen_read, callback_gatt_listen_error);
    if (gatt->proc_ev == NULL)
    {
        printf("[GATT][ERROR] popen event binding failed!\n");
        do_gatt_listen_stop(gatt);
        return -2;
    }
    
    // Create timerfd
    fd = evhr_event_create_timer();
    if (fd < 0)
    {
        printf("[GATT][ERROR] timer create failed!\n");
        do_gatt_listen_stop(gatt);
        return -3;
    }

    // Add timer into event handler
    gatt->time_ev = evhr_event_add_timer_periodic(
            g_mmb_ctx.evhr, fd,
            MMB_GATT_LISTEN_TIMEOUT_SEC, 10, /* MMB_GATT_LISTEN_TIMEOUT_SEC + 10 nsec timeout */
            gatt, callback_gatt_listen_timeout);
    if (gatt->time_ev == NULL)
    {
        printf("[GATT][ERROR] timer event binding failed!\n");
        do_gatt_listen_stop(gatt);
        return -4;
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

int mmb_gatt_send_user_info(struct mmb_user_info_s * user, char * miband_mac)
{
    uint8_t byte;

    // Generate User Info Code, CRC8(0~18 Byte) ^ MAC[last byte]
    byte = atol(miband_mac + strlen(miband_mac) - 2) & 0xFF;
    g_mmb_ctx.user_info.code = crc8(0x00, user->data, sizeof(user->data) - 1) ^ byte;

    return mmb_gatt_send_char_hnd_write_req(
            MIBAND_CHAR_HND_USERINFO, 
            user->data, 
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
