#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "wpool.h"
#include "mmb_ctx.h"
#include "mmb_debug.h"
#include "mmb_miband.h"

extern MMB_CTX g_mmb_ctx;

static void mmb_miband_reset_data(MMB_MIBAND * this)
{
    // Reset some miband data
    memset(&this->sensor,   0, sizeof(struct mmb_sensor_data_s));
    memset(&this->battery,  0, sizeof(struct mmb_battery_data_s));
    memset(&this->realtime, 0, sizeof(struct mmb_realtime_data_s));
}

static void keeplive_timeout_cb(EVHR_EVENT ev)
{
    MMB_MIBAND * this = ev->cb_data;
    mmb_miband_send_battery_read(this);
    mmb_miband_send_sensor_notify(this, 1);
    mmb_miband_send_realtime_notify(this, 1);
}

static void mmb_miband_keeplive_kick(MMB_MIBAND * this)
{
    evhr_event_set_timer(
            this->ev_keeplive, MMB_MIBAND_KEEPLIVE_SEC, 0, 0);
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

    // Bind keeplive timer event
    if ((this->ev_keeplive = evhr_event_add_timer_periodic(
            g_mmb_ctx->evhr, MMB_MIBAND_KEEPLIVE_SEC, 0,
            this, keeplive_timeout_cb)) == NULL)
    {
        printf("[MMB][MIBAND] ERR: Bind WatchDog timer event failed!\n");
    }

    // Start LED control
    if (mmb_miband_led_start(this, g_mmb_ctx->evhr) < 0)
    {
        printf("[MMB][MIBAND] ERR: Start MIBAND LED Control failed!\n");
    }
    ret = mmb_miband_led_mode_change(this, 1);

    MMB_LOG("[MIBAND]", "Connected Task Finish.");

    return;

error_hanlde:

    MMB_LOG("[MIBAND]", "ERR: Connected Task failed! Need to restart connect!");
    mmb_miband_stop(this);
}

static void ble_write_cb(EVHR_EVENT ev)
{
    MMB_MIBAND * this = ev->cb_data;

    int result;
    socklen_t result_len = sizeof(result);

    // OnConnecting Check
    if (this->device.sock_status == EBLE_SOCK_STATUS_CONNECTING)
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
        this->device.sock_status = EBLE_SOCK_STATUS_CONNECTED;
        wpool_add(g_mmb_ctx->wpool, (WPOOL_JOB_FUNC) do_task_connected, this);
        MMB_LOG("[MIBAND]", "Connected.");
    }

    // Only Write OnConnected
    if (this->device.sock_status != EBLE_SOCK_STATUS_CONNECTED)
        return;

}

static void ble_read_cb(EVHR_EVENT ev)
{
    
    MMB_MIBAND * this = ev->cb_data;
    eble_gatt_data_t data;

    if (eble_gatt_recv_data(&this->device, &data) != EBLE_RTN_SUCCESS)
        return;

    switch (data.opcode)
    {
        case EBLE_GATT_OPCODE_ERROR:
            mmb_miband_op_error(this, data.hnd, data.error_code);
            break;
        case EBLE_GATT_OPCODE_READ_TYPE_RESP:
            mmb_miband_op_read_type_resp(this, data.hnd, data.val, data.size);
            break;
        case EBLE_GATT_OPCODE_READ_RESP:
            mmb_miband_op_read_resp(this, data.val, data.size);
            break;
        case EBLE_GATT_OPCODE_WRITE_RESP:
            mmb_miband_op_write_resp(this);
            break;
        case EBLE_GATT_OPCODE_NOTIFICATION:
            mmb_miband_op_notification(this, data.hnd, data.val, data.size);
            break;
        default:
            MMB_DBG("[MIBAND][OP]", "Unknow opcode from MIBAND.");
            return;
    }
}

static void ble_error_cb(EVHR_EVENT ev)
{
    MMB_LOG("[MIBAND]", "BLE Error!");
    mmb_miband_stop((MMB_MIBAND *)ev->cb_data);
}

int mmb_miband_probe(EBleDevice device)
{
    if (strncmp(device->eir.LocalName, "MI", 2) != 0)
        return -1;

    MMB_LOG("[MIBAND]", "Probe by EIR LocalName[%s]", device->eir.LocalName);

    return 0;
}

int mmb_miband_init(EBleDevice * device)
{
    MMB_MIBAND * this = NULL;
    
    MMB_LOG("[MIBAND]", "Initial process running ...");

    if ((this = malloc(sizeof(MMB_MIBAND))) == NULL)
        return -1;

    // Copy Device data
    memcpy(this, *device, sizeof(struct eble_device_ctx_s));

    // Inital miband data
    memset( this + sizeof(struct eble_device_ctx_s), 
            0, 
            sizeof(MMB_MIBAND) - sizeof(struct eble_device_ctx_s));

    this->user.uid       = 19820610;
    this->user.gender    = 1;
    this->user.age       = 32;
    this->user.height    = 175;
    this->user.weight    = 60;
    this->user.type      = 0;
    strcpy((char *)this->user.alias, "RobinMI");

    mmb_miband_reset_data(this);

    free(*device);
    *device = &this->device;

    return 0;
}

int mmb_miband_start(MMB_MIBAND * this, EBleAdapter adapter, EVHR_CTX evhr)
{
    MMB_LOG("[MIBAND]", "Start process running ...");

    // Create BLE connect
    if (eble_device_connect(&this->device, adapter) != EBLE_RTN_SUCCESS)
    {
        printf("[MMB][MIBAND] ERR: Connect MiBand failed!\n");
        return -1;
    }

    // Bind BLE event into evhr
    if ((this->ev_ble = evhr_event_add(
            evhr, this->device.sock, EVHR_EVENT_TYPE_SOCKET, EVHR_ET_MODE,
            this, ble_read_cb, ble_write_cb, ble_error_cb)) == NULL)
    {
        printf("[MMB][MIBAND] ERR: Bind MiBand event failed!\n");
        mmb_miband_stop(this);
        return -2;
    }

    printf("[MMB][MIBAND] Connecting.\n");

    return 0;
}

int mmb_miband_stop(MMB_MIBAND * this)
{
    MMB_LOG("[MIBAND]", "Stop process running ...");

    if (this == NULL)
        return -1;

    if (this->device.sock_status == EBLE_SOCK_STATUS_CLOSED)
        return -2;

    if (this->ev_ble) {
        evhr_event_del(this->ev_ble);
        this->ev_ble = NULL;
    }

    if (this->ev_keeplive) {
        evhr_event_del(this->ev_keeplive);
        this->ev_keeplive = NULL;
    }
    
    mmb_miband_led_stop(this);

    eble_device_disconnect(&this->device);

    this->device.sock_status = EBLE_SOCK_STATUS_CLOSED;
    
    MMB_LOG("[MIBAND]", "Stop process finish");

    return 0;
}

