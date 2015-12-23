#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "mmb_ctx.h"
#include "mmb_ble.h"
#include "mmb_miband.h"

static void do_task_update_timeout(MMB_CTX * mmb)
{
    evhr_event_set_timer(
            mmb->ev_timeout->fd, 
            MMB_MIBAND_TIMEOUT_SEC, 10, 0);
}

static void led_timer_cb(EVHR_EVENT * ev)
{
    MMB_CTX * mmb = ev->pdata;

    // Flush LED
    mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_BLUE);

    // Bind next timer
    evhr_event_set_timer(ev->fd, 5, 0, 1);
}

static void do_task_connected(MMB_CTX * mmb)
{
    int ret;
    int timerfd;

    printf("[MMB][MIBAND] Connected Task.\n");

    /* Auth User Data */
    if ((ret = mmb_miband_send_auth(mmb)) < 0)
    {
        printf("[MMB][MIBAND][ERR] mmb_miband_send_auth failed! ret[%d]\n", ret);
        goto error_hanlde;
    }

    /* Disable all notify */
    ret = mmb_miband_send_sensor_notify(mmb, 0);
    ret = mmb_miband_send_realtime_notify(mmb, 0);
    ret = mmb_miband_send_battery_notify(mmb, 0);

    /* Off LED and Vibration */
    ret = mmb_miband_send_ledcolor(mmb, MMB_LED_COLOR_OFF);
    ret = mmb_miband_send_vibration(mmb, MMB_VIBRATION_STOP);

    /* Enable all notify */
    ret = mmb_miband_send_sensor_notify(mmb, 1);
    ret = mmb_miband_send_realtime_notify(mmb, 1);
    ret = mmb_miband_send_battery_notify(mmb, 1);

    /* Update all information */
    ret = mmb_miband_send_battery_read(mmb);

    /* Create LED timer */
    if ((timerfd = evhr_event_create_timer()) < 0)
    {
        printf("[MMB][MIBAND][ERR] Create LED Timer failed!\n");
    }
    else
    {
        // Add timer into event handler
        if ((mmb->ev_led_timer = evhr_event_add_timer_once(
                        mmb->evhr, timerfd,
                        1, 0, mmb, led_timer_cb)) == NULL)
        {
            printf("[MMB][MIBAND][ERR] Bind LED Timer event failed!\n");
        }
    }

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

        // Connected
        mmb->status = MMB_STATUS_CONNECTED;
        printf("[MMB][MIBAND] Connected.\n");

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

    static struct mmb_ble_att_data_parser_cb_s parser_cb = {
        .error_cb           = mmb_miband_parser_error,
        .read_type_resp_cb  = mmb_miband_parser_read_type_resp,
        .read_resp_cb       = mmb_miband_parser_read_resp,
        .write_resp_cb      = mmb_miband_parser_write_resp,
        .notify_cb          = mmb_miband_parser_notify,
    };
    
    size = read(ev->fd, buf, MMB_BUFFER_SIZE);
    if (size > 0)
    {
        mmb_ble_att_data_parser(buf, size, &parser_cb, mmb);
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

    printf("[MMB][MIBAND] Start.\n");

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

    // Rest some miband data
    memset(&mmb->data.sensor,   0, sizeof(struct mmb_sensor_data_s));
    memset(&mmb->data.battery,  0, sizeof(struct mmb_battery_data_s));
    memset(&mmb->data.realtime, 0, sizeof(struct mmb_realtime_data_s));

    mmb->status = MMB_STATUS_CONNECTING;
    printf("[MMB][MIBAND] Connecting.\n");

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
    
    if (mmb->ev_led_timer) {
        if (mmb->ev_led_timer->fd > 0) {
            evhr_event_stop_timer(mmb->ev_led_timer->fd);
            close(mmb->ev_led_timer->fd);
        }
        evhr_event_del(mmb->evhr, mmb->ev_led_timer);
        mmb->ev_led_timer = NULL;
    }

    mmb->status = MMB_STATUS_INITIAL;
    evhr_stop(mmb->evhr);

    printf("[MMB][MIBAND] Stop.\n");

    return 0;
}

