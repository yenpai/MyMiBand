#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "mmb_ctx.h"
#include "mmb_debug.h"
#include "mmb_miband.h"

static void mmb_miband_reset_data(MMB_MIBAND * this)
{
    // Reset some miband data
    memset(&this->sensor,   0, sizeof(struct mmb_sensor_data_s));
    memset(&this->battery,  0, sizeof(struct mmb_battery_data_s));
    memset(&this->realtime, 0, sizeof(struct mmb_realtime_data_s));
}

static void mmb_miband_watchdog_kick(MMB_MIBAND * this)
{
    evhr_event_set_timer(
            this->ev_watchdog, MMB_MIBAND_TIMEOUT_SEC, 0, 0);
}

static void do_task_connected(MMB_MIBAND * this)
{
    int ret;

    MMB_LOG("[MIBAND]", "Connected Task running ...");

    /* Auth User Data */
    if ((ret = mmb_miband_send_auth(this)) < 0)
    {
        MMB_LOG("[MIBAND]", "ERR: mmb_miband_send_auth failed! ret = %d", ret);
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

    MMB_LOG("[MIBAND]", "Connected Task Finish.");

    return;

error_hanlde:

    MMB_LOG("[MIBAND]", "ERR: Connected Task failed! Need to restart connect!");
    mmb_miband_stop(this);
}

static void ble_write_cb(EVHR_EVENT * ev)
{
    MMB_MIBAND * this = ev->cb_data;

    int result;
    socklen_t result_len = sizeof(result);

    // OnConnecting Check
    if (this->status == MMB_STATUS_CONNECTING)
    {
        if (getsockopt(ev->fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0 || result != 0) {
            // Connect Error
            MMB_LOG("[MIBAND]", "ERR: Connect failed! " 
                    "getsockopt res = %d, errno = %d", 
                    result, errno);
            mmb_miband_stop(this);
            return;
        }

        // Connected
        this->status = MMB_STATUS_CONNECTED;
        MMB_LOG("[MIBAND]", "Connected.");

        do_task_connected(this);
    }

    // Only Write OnConnected
    if (this->status != MMB_STATUS_CONNECTED)
        return;

    mmb_miband_watchdog_kick(this);
}

static void ble_read_cb(EVHR_EVENT * ev)
{
    
    MMB_MIBAND * this = ev->cb_data;
    EBLE_ATTREAD_DATA data;

    if (eble_attread_data(&this->device, &data) != EBLE_RTN_SUCCESS)
        return;

    switch (data.opcode)
    {
        case EBLE_ATTREAD_OP_ERROR:
            mmb_miband_op_error(this, data.hnd, data.error_code);
            break;
        case EBLE_ATTREAD_OP_RESP_READ_TYPE:
            mmb_miband_op_read_type_resp(this, data.hnd, data.val, data.size);
            break;
        case EBLE_ATTREAD_OP_RESP_READ:
            mmb_miband_op_read_resp(this, data.val, data.size);
            break;
        case EBLE_ATTREAD_OP_RESP_WRITE:
            mmb_miband_op_write_resp(this);
            break;
        case EBLE_ATTREAD_OP_NOTIFICATION:
            mmb_miband_op_notification(this, data.hnd, data.val, data.size);
            break;
        default:
            MMB_DBG("[MIBAND][OP]", "Unknow opcode from MIBAND.");
            return;
    }

    mmb_miband_watchdog_kick(this);
}

static void ble_error_cb(EVHR_EVENT * ev)
{
    MMB_LOG("[MIBAND]", "BLE Error!");
    mmb_miband_stop((MMB_MIBAND *)ev->cb_data);
}

static void watchdog_timeout_cb(EVHR_EVENT * ev)
{
    MMB_LOG("[MIBAND]", "BLE timeout!");
    mmb_miband_stop((MMB_MIBAND *)ev->cb_data);
}

int mmb_miband_probe(EBLE_DEVICE * device)
{
    if (strncmp(device->eir.LocalName, "MI", 2) != 0)
        return -1;

    MMB_LOG("[MIBAND]", "Probe by EIR LocalName[%s]", device->eir.LocalName);

    return 0;
}

int mmb_miband_init(EBLE_DEVICE ** device)
{
    MMB_MIBAND * this = NULL;
    
    MMB_LOG("[MIBAND]", "Initial process running ...");

    if ((this = malloc(sizeof(MMB_MIBAND))) == NULL)
        return -1;

    // Copy Device data
    memcpy(this, *device, sizeof(EBLE_DEVICE));

    // Inital miband data
    memset(this + sizeof(EBLE_DEVICE), 0, sizeof(MMB_MIBAND) - sizeof(EBLE_DEVICE));

    this->user.uid       = 19820610;
    this->user.gender    = 1;
    this->user.age       = 32;
    this->user.height    = 175;
    this->user.weight    = 60;
    this->user.type      = 0;
    strcpy((char *)this->user.alias, "RobinMI");

    mmb_miband_reset_data(this);

    *device = &this->device;
    return 0;
}

int mmb_miband_start(MMB_MIBAND * this, EBLE_ADAPTER * adapter, EVHR_CTX * evhr)
{
    MMB_LOG("[MIBAND]", "Start process running ...");

    // Create BLE connect
    if (eble_device_connect(&this->device, adapter) != EBLE_RTN_SUCCESS)
    {
        printf("[MMB][MIBAND] ERR: Connect MiBand failed!\n");
        return -1;
    }

    // Bind BLE event into evhr
    if ((this->ev_ble = evhr_event_add_socket(
            evhr, this->device.sock, this, 
            ble_read_cb, ble_write_cb, ble_error_cb)) == NULL)
    {
        printf("[MMB][MIBAND] ERR: Bind MiBand event failed!\n");
        mmb_miband_stop(this);
        return -2;
    }

    // Bind Watchdog timer event
    if ((this->ev_watchdog = evhr_event_add_timer_periodic(
            evhr, MMB_MIBAND_TIMEOUT_SEC, 0,
            this, watchdog_timeout_cb)) == NULL)
    {
        printf("[MMB][MIBAND] ERR: Bind WatchDog timer event failed!\n");
        mmb_miband_stop(this);
        return -3;
    }

    // Start LED control
    if (mmb_miband_led_start(this, evhr) < 0)
    {
        printf("[MMB][MIBAND] ERR: Start MIBAND LED Control failed!\n");
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
    MMB_LOG("[MIBAND]", "Stop process running ...");

    if (this == NULL)
        return -1;

    if (this->status == MMB_STATUS_STOPPED)
        return -2;

    if (this->ev_ble) {
        evhr_event_del(this->ev_ble);
        this->ev_ble = NULL;
    }

    if (this->ev_watchdog) {
        evhr_event_del(this->ev_watchdog);
        this->ev_watchdog = NULL;
    }
    
    mmb_miband_led_stop(this);

    eble_device_disconnect(&this->device);

    this->status = MMB_STATUS_STOPPED;
    
    MMB_LOG("[MIBAND]", "Stop process finish");

    return 0;
}

