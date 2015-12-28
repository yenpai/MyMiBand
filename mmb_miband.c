#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "mmb_ctx.h"
#include "mmb_ble.h"
#include "mmb_miband.h"

static void mmb_miband_reset_data(MMB_MIBAND * this)
{
    // Reset some miband data
    memset(&this->sensor,   0, sizeof(struct mmb_sensor_data_s));
    memset(&this->battery,  0, sizeof(struct mmb_battery_data_s));
    memset(&this->realtime, 0, sizeof(struct mmb_realtime_data_s));
}

static void mmb_miband_update_timeout(MMB_MIBAND * this)
{
    evhr_event_set_timer(
            this->ev_timeout, MMB_MIBAND_TIMEOUT_SEC, 0, 0);
}

static void do_task_connected(MMB_MIBAND * this)
{
    int ret;

    printf("[MMB][MIBAND][TASK] Connected Task Start.\n");

    /* Auth User Data */
    if ((ret = mmb_miband_send_auth(this)) < 0)
    {
        printf("[MMB][MIBAND][TASK] ERR: mmb_miband_send_auth failed! ret[%d]\n", ret);
        goto error_hanlde;
    }

    /* Disable all notify */
    ret = mmb_miband_send_sensor_notify(this, 0);
    ret = mmb_miband_send_realtime_notify(this, 0);
    ret = mmb_miband_send_battery_notify(this, 0);

    /* Off LED and Vibration */
    ret = mmb_miband_send_ledcolor(this, MMB_LED_COLOR_OFF);
    ret = mmb_miband_send_vibration(this, MMB_VIBRATION_STOP);

    /* Enable all notify */
    ret = mmb_miband_send_sensor_notify(this, 1);
    ret = mmb_miband_send_realtime_notify(this, 1);
    ret = mmb_miband_send_battery_notify(this, 1);

    /* Update all information */
    ret = mmb_miband_send_battery_read(this);

    /* LED mode */
    ret = mmb_miband_led_mode_change(this, 1);

    printf("[MMB][MIBAND][TASK] Connected Task Finish.\n");

    return;

error_hanlde:

    printf("[MMB][MIBAND][TASK] ERR: Need to restart connect!\n");
    mmb_miband_stop(this);
}


static void ble_write_cb(EVHR_EVENT * ev)
{
    MMB_MIBAND * this = ev->pdata;

    int result;
    socklen_t result_len = sizeof(result);

    // OnConnecting Check
    if (this->status == MMB_STATUS_CONNECTING)
    {
        if (getsockopt(ev->fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0 || result != 0) {
            // Connect Error
            mmb_miband_stop(this);
            return;
        }

        // Connected
        this->status = MMB_STATUS_CONNECTED;
        printf("[MMB][MIBAND] Connected.\n");

        do_task_connected(this);
    }

    // Only Write OnConnected
    if (this->status != MMB_STATUS_CONNECTED)
        return;

    mmb_miband_update_timeout(this);
}

static void ble_read_cb(EVHR_EVENT * ev)
{
    
    MMB_MIBAND * this = ev->pdata;
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
        mmb_ble_att_data_parser(buf, size, &parser_cb, this);
        mmb_miband_update_timeout(this);
    }

}

static void ble_error_cb(EVHR_EVENT * ev)
{
    printf("[MMB][MIBAND] BLE Error!\n");
    mmb_miband_stop((MMB_MIBAND *)ev->pdata);
}

static void ble_timeout_cb(EVHR_EVENT * ev)
{
    printf("[MMB][MIBAND] BLE timeout!\n");
    mmb_miband_stop((MMB_MIBAND *)ev->pdata);
}

int mmb_miband_probe(struct mmb_ble_device_base_s * device)
{
    if (strncmp(device->name, "MI", 2) != 0)
        return -1;

    printf( "[MMB][MIBAND][PROBE] \t\t==> "
            "MiBand Device: Name[%s].\n", device->name);

    return 0;
}

int mmb_miband_init(MMB_MIBAND * this, bdaddr_t * dest, struct evhr_ctx_s * evhr)
{

    memset(this, 0, sizeof(MMB_MIBAND));
    bacpy(&this->addr, dest);
    this->evhr = evhr;

    //str2ba("88:0F:10:2A:5F:08", &this->addr);
    this->user.uid       = 19820610;
    this->user.gender    = 1;
    this->user.age       = 32;
    this->user.height    = 175;
    this->user.weight    = 60;
    this->user.type      = 0;
    strcpy((char *)this->user.alias, "RobinMI");

    mmb_miband_reset_data(this);

    return 0;
}

int mmb_miband_start(MMB_MIBAND * this, bdaddr_t * src)
{
    int sock = -1;

    printf("[MMB][MIBAND] Start.\n");

    // Create BLE connect
    if ((sock = mmb_ble_connect(src, &this->addr)) < 0)
    {
        printf("[MMB][MIBAND][ERR] Connect MiBand failed!\n");
        return -1;
    }

    // Bind BLE event into evhr
    if ((this->ev_ble = evhr_event_add_socket(
                    this->evhr, sock, 
                    this, ble_read_cb, ble_write_cb, ble_error_cb)) == NULL)
    {
        printf("[MMB][MIBAND][ERR] Bind MiBand event failed!\n");
        mmb_miband_stop(this);
        return -2;
    }

    // Add timer into event handler
    if ((this->ev_timeout = evhr_event_add_timer_periodic(
            this->evhr, MMB_MIBAND_TIMEOUT_SEC, 10, 
            this, ble_timeout_cb)) == NULL)
    {
        printf("[MMB][MIBAND][ERROR] Bind Timer event failed!\n");
        mmb_miband_stop(this);
        return -4;
    }

    // Start LED control
    if (mmb_miband_led_start(this) < 0)
    {
        printf("[MMB][MIBAND][ERROR] Start MIBAND LED Control failed!\n");
        mmb_miband_stop(this);
        return -5;
    }

    // Reset some miband data
    mmb_miband_reset_data(this);

    this->status = MMB_STATUS_CONNECTING;
    printf("[MMB][MIBAND] Connecting.\n");

    return 0;
}

int mmb_miband_stop(MMB_MIBAND * this)
{
    if (this == NULL)
        return -1;

    if (this->status == MMB_STATUS_STOPPED)
        return -2;

    if (this->ev_ble) {
        if (this->ev_ble->fd > 0) {
            close(this->ev_ble->fd);
        }
        evhr_event_del(this->ev_ble);
        this->ev_ble = NULL;
    }

    if (this->ev_timeout) {
        evhr_event_del(this->ev_timeout);
        this->ev_timeout = NULL;
    }
    
    mmb_miband_led_stop(this);

    this->status = MMB_STATUS_STOPPED;
    printf("[MMB][MIBAND] Stopped.\n");

    // TODO: Hard Code to stop here, need to implement callback
    evhr_stop(this->evhr);

    return 0;
}

