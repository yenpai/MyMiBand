#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "mmb_ctx.h"
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

static int mmb_miband_update_battery_data(MMB_CTX * mmb, uint8_t * buf, size_t size)
{
    struct mmb_battery_data_s * old = &mmb->data.battery;
    size_t data_size = sizeof(struct mmb_battery_data_s);

    if (size < data_size)
    {
        printf("[MMB][MIBAND][BATTERY] data size[%ld] less than %lu bytes, ignore.\n", 
                size, data_size);
        return -1;
    }
    
    if (memcmp(old, buf, data_size) == 0)
    {
        printf("[MMB][MIBAND][BATTERY] data no change, ignore.\n");
        return -2;
    }

    memcpy(old, buf, data_size);
    
    printf("[MMB][MIBAND][BATTERY][UPDATE]");
    printf(" Level[%d]", old->level);
    printf(" Date[%d-%d-%d]", old->dt_year, old->dt_month, old->dt_day);
    printf(" Time[%d:%d:%d]", old->dt_hour, old->dt_minute, old->dt_second);
    printf(" Cycle[%d]", old->cycle);
    printf(" Status[%d]\n", old->status);
    
    return 0;
}

static int mmb_miband_update_sensor_data(MMB_CTX * mmb, uint8_t * buf, size_t size)
{
    struct mmb_sensor_data_s * old = &mmb->data.sensor;
    struct mmb_sensor_data_s new; 
    size_t data_size = sizeof(struct mmb_sensor_data_s);
    int diff_x, diff_y, diff_z;
    uint8_t action = 0;

    if (size < data_size)
    {
        printf("[MMB][MIBAND][SENSOR] data size[%ld] less than %lu bytes, ignore.\n", 
                size, data_size);
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
        diff_x = (new.x - old->x);
        diff_y = (new.y - old->y);
        diff_z = (new.z - old->z);

        // ignore x/y/z diff less 64
        if (abs(diff_x) > 64)
            action |= 0x1;
        if (abs(diff_y) > 64)
            action |= 0x2;
        if (abs(diff_z) > 64)
            action |= 0x4;

        if (action)
        {
            // Send Notify!
            printf("[MMB][MIBAND][SENSOR][UPDATE] X[%d] Y[%d] Z[%d]\n", diff_x, diff_y, diff_z);
        }
    }

    memcpy(old, &new, data_size);
    return 0;
}

static int mmb_miband_parsing_notification(MMB_CTX * mmb, uint16_t hnd, uint8_t *val, size_t size)
{
    int ret = 0;
    size_t i = 0;

    switch (hnd)
    {
        case MMB_PF_HND_REALTIME:
            printf("[MMB][MIBAND][REALTIME][UPDATE] \n");
            break;

        case MMB_PF_HND_BATTERY:
            ret = mmb_miband_update_battery_data(mmb, val, size);
            break;

        case MMB_PF_HND_SENSOR:
            ret = mmb_miband_update_sensor_data(mmb, val, size);
            break;

        default:
            printf("[MMB][MIBAND][NOTIFICATION] unknow hnd[0x%04x] size[%ld] ", hnd, size);
            for (i=0;i<size;i++)
                printf("%02x", val[i]);
            printf("\n");
            ret = -1;
            break;
    }

    return ret;
}

int mmb_miband_parsing_raw_data(MMB_CTX * mmb, uint8_t * buf, size_t size)
{
    size_t i = 0;
    int ret = 0;

    switch (buf[0])
    {
        case MMB_BLE_ATT_OPCODE_ERROR:
            printf("[MMB][MIBAND][ERROR]");
            printf(" ReqOpcode[0x%02x]", buf[1]);
            printf(" Hnd[0x%02x%02x]", buf[3], buf[2]);
            printf(" ErrCode[0x%02x]", buf[4]);
            printf("\n");
            ret = -1;
            break;

        case MMB_BLE_ATT_OPCODE_READ_RESP:
            printf("[MMB][MIBAND][READ_RESP]\n\t");
            for (i=1;i<size;i++)
                printf("%02x ", buf[i]);
            printf("\n");
            break;

        case MMB_BLE_ATT_OPCODE_WRITE_RESP:
            //printf("[MMB][MIBAND][WRITE_RESP]\n");
            break;

        case MMB_BLE_ATT_OPCODE_NOTIFICATION:
            ret = mmb_miband_parsing_notification(
                    mmb, (uint16_t) buf[1], buf + 3, size - 3);
            break;
    }

    return ret;
}

int mmb_miband_send_auth(MMB_CTX * mmb)
{
    int ret;
    uint8_t * ptr   = NULL;
    size_t size     = 0;

    ptr   = (uint8_t *) &mmb->data.user;
    size  = sizeof(struct mmb_user_data_s);

    // Generate User Info Code, CRC8(0~18 Byte) ^ MAC[last byte]
    mmb->data.user.code = crc8(0x00, ptr, size - 1) ^ (mmb->data.addr.b[0] & 0xFF);

    ret = mmb_ble_write_req(mmb->ev_ble->fd, MMB_PF_HND_USER, ptr, size);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_realtime_notify(MMB_CTX * mmb, uint8_t enable)
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
    ret = mmb_ble_write_req(mmb->ev_ble->fd, MMB_PF_HND_REALTIME_NOTIFY, value, 2);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_sensor_notify(MMB_CTX * mmb, uint8_t enable)
{
    int ret = 0;
    uint8_t value[2] = {0x00, 0x00};
    
    if (enable)
        value[0] = enable;

    // Disable Data
    ret = mmb_ble_write_req(mmb->ev_ble->fd, MMB_PF_HND_SENSOR, value, 2);
    if (ret < 0)
        return ret;

    // Disable Notify
    ret = mmb_ble_write_req(mmb->ev_ble->fd, MMB_PF_HND_SENSOR_NOTIFY, value, 2);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_battery_notify(MMB_CTX * mmb, uint8_t enable)
{
    int ret = 0;
    uint8_t value[2] = {0x00, 0x00};

    if (enable)
        value[0] = enable;

    //ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_BATTERY, value, 2);
    //if (ret < 0)
    //    return ret;

    ret = mmb_ble_write_req(mmb->ev_ble->fd, MMB_PF_HND_BATTERY_NOTIFY, value, 2);
    if (ret < 0)
        return ret;

    return 0;
}

int mmb_miband_send_vibration(MMB_CTX * mmb, uint8_t mode)
{
    /*
     * VIBRATION_STOP
     * VIBRATION_WITH_LED
     * VIBRATION_10_TIMES_WITH_LED
     * VIBRATION_WITHOUT_LED
     */

    uint8_t buf[1];
    buf[0] = mode;
    return mmb_ble_write_cmd(mmb->ev_ble->fd, MMB_PF_HND_VIBRATION, buf, 1);
}

int mmb_miband_send_ledcolor(MMB_CTX * mmb, uint32_t color)
{
    uint8_t buf[5] = {14,0,0,0,0};

    buf[1] = 0xFF & color      ; // R
    buf[2] = 0xFF & color >> 8 ; // G
    buf[3] = 0xFF & color >> 16; // B
    buf[4] = 0xFF & color >> 24; // onoff

    return mmb_ble_write_req(mmb->ev_ble->fd, MMB_PF_HND_CONTROL, buf, 5);
}

int mmb_miband_send_battery_read(MMB_CTX * mmb)
{
    return mmb_ble_read_req(mmb->ev_ble->fd, MMB_PF_HND_BATTERY);
}
