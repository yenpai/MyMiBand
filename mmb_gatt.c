#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mmb_ctx.h"
#include "mmb_util.h"
#include "evhr.h"

static void do_update_mmb_sensor_data(MMB_CTX * mmb, uint16_t seq, uint16_t x, uint16_t y, uint16_t z)
{

    struct mmb_sensor_data_s *new, *old;
    
    new = &mmb->data.sensor;
    old = &mmb->data.sensor_old;

    printf("[GATT][NOTIFY][SENSOR] SEQ(%u) X(%u) Y(%u) Z(%u)\n", 
            seq, x, y, z);

    // backup to old sensor data
    if (new->seq != 0 && new->x != 0 && new->y != 0)
        memcpy(old, new, sizeof(struct mmb_sensor_data_s));

    // update new sensor data
    new->seq = seq;
    new->x   = x;
    new->y   = y;
    new->z   = z;

    // check change
    if (old->seq != 0 && old->x != 0 && old->y != 0)
    {
        printf("[GATT][NOTIFY][SENSOR][CHANGE] X(%d) Y(%d) Z(%d)\n",
                (new->x - old->x),
                (new->y - old->y),
                (new->z - old->z));
    }
}

static int do_gatt_listen_string_parsing(MMB_CTX * mmb, char * str)
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
            case MMB_PF_SENSOR_HND:
                buf_len = hex_str_split_to_bytes(buf, sizeof(buf), tmp, " ");
                if (buf_len >= 8)
                    do_update_mmb_sensor_data(
                            mmb,
                            buf[0] | buf[1] << 8,
                            buf[2] | buf[3] << 8,
                            buf[4] | buf[5] << 8,
                            buf[6] | buf[7] << 8);
                else
                    printf("[GATT][NOTIFY][SENSOR] Data Len Not enought! [%s]\n", tmp);

                break;
            default:
                printf("[GATT][NOTIFY][0x%04x] %s\n", hnd, tmp);
                break;
        }
        return 0;
    }

    return -1;
}

static int do_gatt_listen_buffer_parsing(MMB_CTX * mmb)
{
    struct mmb_gatt_s * gatt = &mmb->gatt;
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
        if (do_gatt_listen_string_parsing(mmb, curr) != -1)
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

static void callback_gatt_listen_timeout(EVHR_EVENT * event)
{
    MMB_CTX * mmb = (MMB_CTX *) event->pdata;;
    printf("[GATT][TIMEOUT].\n");
    do_gatt_listen_stop(&mmb->gatt);
}

static void callback_gatt_listen_read(EVHR_EVENT * event)
{
    MMB_CTX * mmb = (MMB_CTX *) event->pdata;;
    struct mmb_gatt_s * gatt = &mmb->gatt;
    int len = 0;
    char buf[CMD_BUFFER_SIZE];

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
        do_gatt_listen_buffer_parsing(mmb);

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

static void callback_gatt_listen_error(EVHR_EVENT * event)
{
    MMB_CTX * mmb = (MMB_CTX *) event->pdata;
    printf("[GATT][ERROR].\n");
    do_gatt_listen_stop(&mmb->gatt);
}

int mmb_gatt_listen_start(MMB_CTX * mmb)
{
    int fd = -1;
    char shell_cmd[CMD_BUFFER_SIZE];
    struct mmb_gatt_s * gatt = &mmb->gatt;
    
    gatt->popen_fd      = NULL;
    gatt->proc_ev       = NULL;
    gatt->time_ev       = NULL;
    gatt->buf_size      = 0;
    gatt->is_running    = 0;
    gatt->pdata         = (void *) mmb;

    // Start Listen
    snprintf(shell_cmd, CMD_BUFFER_SIZE, "%s -i %s -b %s --char-read -a 0x%04x --listen;", 
            CMD_GATTTOOL_PATH, mmb->hci_dev, mmb->data.miband_mac, MMB_PF_USER_HND);
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
            mmb->evhr, fd, mmb,
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
            mmb->evhr, fd,
            MMB_GATT_LISTEN_TIMEOUT_SEC, 10, /* MMB_GATT_LISTEN_TIMEOUT_SEC + 10 nsec timeout */
            mmb, callback_gatt_listen_timeout);
    if (gatt->time_ev == NULL)
    {
        printf("[GATT][ERROR] timer event binding failed!\n");
        do_gatt_listen_stop(gatt);
        return -4;
    }

    mmb->gatt.is_running = 1;

    return 0;
}

int mmb_gatt_send_char_hnd_write_req(MMB_CTX * mmb, uint8_t hnd, uint8_t * data, size_t size)
{
    char cmd[CMD_BUFFER_SIZE];
    char buf[CMD_BUFFER_SIZE];

    if ((size * 2) > (CMD_BUFFER_SIZE - 1))
        return -1;

    bytes_to_hex_str(buf, data, size);

    snprintf(cmd, CMD_BUFFER_SIZE, 
            "%s -i %s -b %s --char-write-req -a 0x%04x -n %s;", 
            CMD_GATTTOOL_PATH, mmb->hci_dev, mmb->data.miband_mac, hnd, buf);
    printf("[GATT][CMD] %s\n", cmd);

    return system(cmd);
} 

int mmb_gatt_send_char_hnd_write_cmd(MMB_CTX * mmb, uint8_t hnd, uint8_t * data, size_t size)
{
    char cmd[CMD_BUFFER_SIZE];
    char buf[CMD_BUFFER_SIZE];

    if ((size * 2) > (CMD_BUFFER_SIZE - 1))
        return -1;

    bytes_to_hex_str(buf, data, size);

    snprintf(cmd, CMD_BUFFER_SIZE, 
            "%s -i %s -b %s --char-write -a 0x%04x -n %s;", 
            CMD_GATTTOOL_PATH, mmb->hci_dev, mmb->data.miband_mac, hnd, buf);
    printf("[GATT][CMD] %s\n", cmd);

    return system(cmd);
} 
