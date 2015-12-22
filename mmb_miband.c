#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "mmb_ctx.h"
#include "mmb_miband.h"

extern int mmb_ble_connect(const bdaddr_t * src, const bdaddr_t * dst);

static void do_task_update_timeout(MMB_CTX * mmb)
{
    evhr_event_set_timer(
            mmb->ev_timeout->fd, 
            MMB_MIBAND_TIMEOUT_SEC, 10, 0);
}

static void do_task_connected(MMB_CTX * mmb)
{
    int ret;

    /* Auth User Data */
    if ((ret = mmb_miband_send_auth(mmb)) < 0)
    {
        printf("[MMB][TASK][ERR] mmb_miband_send_auth failed! ret[%d]\n", ret);
        goto error_hanlde;
    }

#if 0
    /* Disable all notify */
    ret = mmb_miband_send_sensor_notify(mmb, 0);
    ret = mmb_miband_send_realtime_notify(mmb, 0);
    ret = mmb_miband_send_battery_notify(mmb, 0);

    /* Enable all notify */
    ret = mmb_miband_send_sensor_notify(mmb, 1);
    ret = mmb_miband_send_realtime_notify(mmb, 1);
    ret = mmb_miband_send_battery_notify(mmb, 1);
#endif


    //mmb_miband_send_vibration(mmb, 1);
    //sleep(5);

    do_task_update_timeout(mmb);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, 0x01000007UL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x01000008UL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x01000009UL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x0100000AUL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x0100000BUL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x0100000CUL);
    usleep(800000);
    mmb_miband_send_ledcolor(mmb, 0x0100000BUL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x0100000AUL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x01000009UL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x01000008UL);
    usleep(100000);
    mmb_miband_send_ledcolor(mmb, 0x01000007UL);
    usleep(100000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_GREEN);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_BLUE);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_YELLOW);
    usleep(200000);
    
    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_ORANGE);
    usleep(200000);
    
    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    usleep(200000);

    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_WHITE);
    usleep(200000);
    
    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    usleep(200000);

    return;

error_hanlde:

    printf("[MMB][TASK][ERR] Need to restart connect!\n");
    mmb_miband_stop(mmb);
}

static void timeout_cb(EVHR_EVENT * ev)
{
    printf("timeout\n");
    mmb_miband_stop((MMB_CTX *)ev->pdata);
}

static void write_cb(EVHR_EVENT * ev)
{
    MMB_CTX * mmb = ev->pdata;

    int result;
    socklen_t result_len = sizeof(result);

    // OnConnecting Check
    if (mmb->status == MMB_STATUS_CONNECTING)
    {
        if (getsockopt(ev->fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0 || result != 0) {
            // Connect Error
            mmb_miband_stop((MMB_CTX *)ev->pdata);
            return;
        }

        // Connecting
        mmb->status = MMB_STATUS_CONNECTED;
        do_task_connected(mmb);
        
    }

    // Only Write OnConnected
    if (mmb->status != MMB_STATUS_CONNECTED)
        return;

    do_task_update_timeout(mmb);
}

static void read_cb(EVHR_EVENT * ev)
{
    
    MMB_CTX * mmb = ev->pdata;
    uint8_t buf[MMB_BUFFER_SIZE];
    int size = 0;
    
    size = read(ev->fd, buf, MMB_BUFFER_SIZE);
    if (size > 0)
    {
        mmb_miband_parsing_raw_data(mmb, buf, size);
        do_task_update_timeout(mmb);
    }
}

static void error_cb(EVHR_EVENT * ev)
{
    printf("Error\n");
    mmb_miband_stop((MMB_CTX *)ev->pdata);
}

int mmb_miband_start(MMB_CTX * mmb)
{
    int sock = -1;
    int timerfd = -1;

    // Create BLE connect
    if ((sock = mmb_ble_connect(&mmb->addr, &mmb->data.addr)) < 0)
    {
        printf("[MMB][MIBAND][ERR] Connect MiBand failed!\n");
        return -1;
    }

    // Bind BLE event into evhr
    if ((mmb->ev_ble = evhr_event_add_socket(
                    mmb->evhr, sock, 
                    mmb, read_cb, write_cb, error_cb)) == NULL)
    {
        printf("[MMB][MIBAND][ERR] Bind MiBand event failed!\n");
        mmb_miband_stop(mmb);
        return -2;
    }

    // Create timeout
    if ((timerfd = evhr_event_create_timer()) < 0)
    {
        printf("[MMB][MIBAND][ERROR] Create Timer failed!\n");
        mmb_miband_stop(mmb);
        return -3;
    }

    // Add timer into event handler
    if ((mmb->ev_timeout = evhr_event_add_timer_periodic(
            mmb->evhr, timerfd,
            MMB_MIBAND_TIMEOUT_SEC, 10, 
            mmb, timeout_cb)) == NULL)
    {
        printf("[MMB][MIBAND][ERROR] Bind Timer event failed!\n");
        mmb_miband_stop(mmb);
        return -4;
    }

    mmb->status = MMB_STATUS_CONNECTING;

    return 0;
}

int mmb_miband_stop(MMB_CTX * mmb)
{
    if (mmb->ev_ble) {
        if (mmb->ev_ble->fd > 0) {
            close(mmb->ev_ble->fd);
        }
        evhr_event_del(mmb->evhr, mmb->ev_ble);
        mmb->ev_ble = NULL;
    }

    if (mmb->ev_timeout) {
        if (mmb->ev_timeout->fd > 0) {
            evhr_event_stop_timer(mmb->ev_timeout->fd);
            close(mmb->ev_timeout->fd);
        }
        evhr_event_del(mmb->evhr, mmb->ev_timeout);
        mmb->ev_timeout = NULL;
    }

    mmb->status = MMB_STATUS_INITIAL;
    evhr_stop(mmb->evhr);

    return 0;
}

