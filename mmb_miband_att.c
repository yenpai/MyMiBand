#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "mmb_miband.h"
#include "mmb_ble.h"
#include "mmb_util.h"

/* PROFILE UUID */
#define MMB_PF_UUID_USER            0xff04
#define MMB_PF_UUID_CONTROL         0xff05
#define MMB_PF_UUID_REALTIME        0xff06
#define MMB_PF_UUID_LEPARAM         0xff09
#define MMB_PF_UUID_BATTERY         0xff0c
#define MMB_PF_UUID_TEST            0xff0d
#define MMB_PF_UUID_SENSOR          0xff0e
#define MMB_PF_UUID_PAIR            0xff0f
#define MMB_PF_UUID_VIBRATION       0x2a06

/* PROFILE Handle */
#define MMB_PF_HND_USER             0x0019
#define MMB_PF_HND_CONTROL          0x001b
#define MMB_PF_HND_REALTIME         0x001d
#define MMB_PF_HND_REALTIME_NOTIFY  0x001e
#define MMB_PF_HND_LEPARAM          0x0025
#define MMB_PF_HND_LEPARAM_NOTIFY   0x0026
#define MMB_PF_HND_BATTERY          0x002c
#define MMB_PF_HND_BATTERY_NOTIFY   0x002d
#define MMB_PF_HND_TEST             0x002f
#define MMB_PF_HND_SENSOR           0x0031
#define MMB_PF_HND_SENSOR_NOTIFY    0x0032
#define MMB_PF_HND_PAIR             0x0034
#define MMB_PF_HND_VIBRATION        0x0051

static int min_diff_value(uint8_t old, uint8_t new)
{
    int d1 = old - new;
    int d2 = new - old;
    
    if (abs(d1) > abs(d2))
        return d2;
    else
        return d1;
}

static int mmb_miband_update_realtime_data(MMB_MIBAND * this, uint8_t * buf, size_t size)
{
    struct mmb_realtime_data_s * old = &this->realtime;
    size_t data_size = sizeof(struct mmb_realtime_data_s);

    if (size < data_size)
    {
        printf("[MMB][MIBAND][UPDATE][REALTIME] data size[%ld] less than %lu bytes, ignore.\n", 
                size, data_size);
        dump_hex_bytes("DATA", buf, size);
        return -1;
    }

    if (memcmp(old, buf, data_size) == 0)
    {
        printf("[MMB][MIBAND][UPDATE][REALTIME] data no change, ignore.\n");
        return -2;
    }

    memcpy(old, buf, data_size);
    printf("[MMB][MIBAND][UPDATE][REALTIME] Step[%d]\n", old->step);

    return 0;
}

static int mmb_miband_update_battery_data(MMB_MIBAND * this, uint8_t * buf, size_t size)
{
    struct mmb_battery_data_s * old = &this->battery;
    size_t data_size = sizeof(struct mmb_battery_data_s);

    if (size < data_size)
    {
        printf("[MMB][MIBAND][UPDATE][BATTERY] data size[%ld] less than %lu bytes, ignore.\n", 
                size, data_size);
        dump_hex_bytes("DATA", buf, size);
        return -1;
    }
    
    if (memcmp(old, buf, data_size) == 0)
    {
        printf("[MMB][MIBAND][UPDATE][BATTERY] data no change, ignore.\n");
        return -2;
    }

    memcpy(old, buf, data_size);
    
    printf("[MMB][MIBAND][UPDATE][BATTERY]");
    printf(" Level[%d]", old->level);
    printf(" Date[%d-%02d-%02d]", 2000 + old->dt_year, old->dt_month, old->dt_day);
    printf(" Time[%02d:%02d:%02d]", old->dt_hour, old->dt_minute, old->dt_second);
    printf(" Cycle[%d]", old->cycle);
    printf(" Status[%d]\n", old->status);
    
    return 0;
}

static int mmb_miband_update_sensor_data(MMB_MIBAND * this, uint8_t * buf, size_t size)
{
    struct mmb_sensor_data_s * old = &this->sensor;
    struct mmb_sensor_data_s new; 
    size_t data_size = sizeof(struct mmb_sensor_data_s);
    int diff_x, diff_y, diff_z, max_abs_diff = 0;

    if (size < data_size)
    {
        printf("[MMB][MIBAND][SENSOR] data size[%ld] less than %lu bytes, ignore.\n", 
                size, data_size);
        dump_hex_bytes("DATA", buf, size);
        return -1;
    }
    memcpy(&new, buf, data_size);

    // Check old data exist
    if (!(old->seq == 0 && old->x == 0 && old->y == 0 && old->z == 0))
    {
        // check seq
        if (old->seq == new.seq)
            return -2;

        // check new/old data diff
        diff_x = min_diff_value(new.x, old->x);
        diff_y = min_diff_value(new.y, old->y);
        diff_z = min_diff_value(new.z, old->z);

        // diff value = -256 ~ 256
        // ignore x/y/z diff less 64
        max_abs_diff = ( abs(diff_x) >= abs(diff_y) ) ? abs(diff_x) : abs(diff_y) ;
        max_abs_diff = ( abs(diff_z) >= max_abs_diff ) ? abs(diff_z) : max_abs_diff ;

        if (max_abs_diff > 64)
        {
            // Send Notify!
            printf("[MMB][MIBAND][UPDATE][SENSOR] Diff - X[%d] Y[%d] Z[%d]\n", diff_x, diff_y, diff_z);

            // Change LED Mode to notify
            if (this->led_mode == 2) {
                mmb_miband_led_mode_change(this, 3);
                this->led_index++;
            } else if (this->led_mode == 3) {

                if (this->led_index > 20)
                {
                    if (max_abs_diff > (128 + 64 + 32))
                        this->led_index += 1;
                }
                else if (this->led_index > 10)
                { 
                    if (max_abs_diff > (128 + 64))
                        this->led_index += 1;
                }
                else
                {
                    this->led_index += 1;
                }
            }
        }
    }

    memcpy(old, &new, data_size);
    return 0;
}

int mmb_miband_parser_error(void *UNUSED(pdata), uint16_t hnd, uint8_t error_code)
{
    printf("[MMB][MIBAND][PARSER-ERR] HND = 0x%04x, ErrCode = %d.\n", hnd, error_code);
    return 0;
}

int mmb_miband_parser_read_type_resp(void *pdata, uint16_t hnd, uint8_t *val, size_t size)
{
    int ret = 0;
    MMB_MIBAND * this = pdata;

    switch (hnd)
    {
        case MMB_PF_HND_BATTERY:
            ret = mmb_miband_update_battery_data(this, val, size);
            break;

        default:
            printf("[MMB][MIBAND][PARSER-ReadTypeResp] unknow HND = 0x%04x.\n", hnd);
            ret = -1;
            break;
    }

    return ret;
}

int mmb_miband_parser_read_resp(void *UNUSED(pdata), uint8_t *UNUSED(val), size_t UNUSED(size))
{
    return 0;
}

int mmb_miband_parser_write_resp(void * UNUSED(pdata))
{
    return 0;
}

int mmb_miband_parser_notify(void *pdata, uint16_t hnd, uint8_t *val, size_t size)
{
    MMB_MIBAND * this = pdata;
    int ret = 0;

    switch (hnd)
    {
        case MMB_PF_HND_REALTIME:
            ret = mmb_miband_update_realtime_data(this, val, size);
            break;

        case MMB_PF_HND_BATTERY:
            ret = mmb_miband_update_battery_data(this, val, size);
            break;

        case MMB_PF_HND_SENSOR:
            ret = mmb_miband_update_sensor_data(this, val, size);
            break;

        default:
            printf("[MMB][MIBAND][NOTIFICATION] unknow hnd[0x%04x] size[%ld] ", hnd, size);
            dump_hex_bytes("DATA", val, size);
            ret = -1;
            break;
    }

    return ret;
}

int mmb_miband_send_auth(MMB_MIBAND * this)
{
    int ret;
    uint8_t * ptr   = NULL;
    size_t size     = 0;

    ptr   = (uint8_t *) &this->user;
    size  = sizeof(struct mmb_user_data_s);

    // Generate User Info Code, CRC8(0~18 Byte) ^ MAC[last byte]
    this->user.code = crc8(0x00, ptr, size - 1) ^ (this->addr.b[0] & 0xFF);

    ret = mmb_ble_write_req(this->ev_ble->fd, MMB_PF_HND_USER, ptr, size);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_realtime_notify(MMB_MIBAND * this, uint8_t enable)
{
    int ret = 0;
    uint8_t value[2] = {0x00, 0x00};
    
    if (enable)
        value[0] = enable;

    // Disable Data
    //ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_REALTIME, value, 2);
    //if (ret < 0)
    //    return ret;

    // Disable Notify
    ret = mmb_ble_write_req(this->ev_ble->fd, MMB_PF_HND_REALTIME_NOTIFY, value, 2);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_sensor_notify(MMB_MIBAND * this, uint8_t enable)
{
    int ret = 0;
    uint8_t value[2] = {0x00, 0x00};
    
    if (enable)
        value[0] = enable;

    // Disable Data
    ret = mmb_ble_write_req(this->ev_ble->fd, MMB_PF_HND_SENSOR, value, 2);
    if (ret < 0)
        return ret;

    // Disable Notify
    ret = mmb_ble_write_req(this->ev_ble->fd, MMB_PF_HND_SENSOR_NOTIFY, value, 2);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_battery_notify(MMB_MIBAND * this, uint8_t enable)
{
    int ret = 0;
    uint8_t value[2] = {0x00, 0x00};

    if (enable)
        value[0] = enable;

    //ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_BATTERY, value, 2);
    //if (ret < 0)
    //    return ret;

    ret = mmb_ble_write_req(this->ev_ble->fd, MMB_PF_HND_BATTERY_NOTIFY, value, 2);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_vibration(MMB_MIBAND * this, uint8_t mode)
{
    /*
     * VIBRATION_STOP
     * VIBRATION_WITH_LED
     * VIBRATION_10_TIMES_WITH_LED
     * VIBRATION_WITHOUT_LED
     */

    uint8_t buf[1];
    buf[0] = mode;
    return mmb_ble_write_cmd(this->ev_ble->fd, MMB_PF_HND_VIBRATION, buf, 1);
}

int mmb_miband_send_ledcolor(MMB_MIBAND * this, uint32_t color)
{
    uint8_t buf[5] = {14,0,0,0,0};

    buf[1] = 0xFF & color      ; // R
    buf[2] = 0xFF & color >> 8 ; // G
    buf[3] = 0xFF & color >> 16; // B
    buf[4] = 0xFF & color >> 24; // onoff

    return mmb_ble_write_req(this->ev_ble->fd, MMB_PF_HND_CONTROL, buf, 5);
}

int mmb_miband_send_battery_read(MMB_MIBAND * this)
{
    return mmb_ble_read_type_req(this->ev_ble->fd, MMB_PF_HND_BATTERY, MMB_PF_UUID_BATTERY);
}

