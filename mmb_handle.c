#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmb_ctx.h"
#include "mmb_util.h"
//#include "evhr.h"

extern int mmb_gatt_send_char_hnd_write_req(MMB_CTX * mmb, uint8_t hnd, uint8_t * data, size_t size);
extern int mmb_gatt_send_char_hnd_write_cmd(MMB_CTX * mmb, uint8_t hnd, uint8_t * data, size_t size);

int mmb_handle_send_auth(MMB_CTX * mmb)
{
    uint8_t byte    = 0x00;
    uint8_t * ptr   = NULL;
    size_t size     = 0;

    ptr   = (uint8_t *) &mmb->data.user;
    size  = sizeof(struct mmb_user_data_s);

    // Generate User Info Code, CRC8(0~18 Byte) ^ MAC[last byte]
    byte = atol(mmb->data.miband_mac + strlen(mmb->data.miband_mac) - 2) & 0xFF;
    mmb->data.user.code = crc8(0x00, ptr, size - 1) ^ byte;

    return mmb_gatt_send_char_hnd_write_req(
            mmb, MMB_PF_USER_HND, ptr, size);
}

int mmb_handle_send_sensor_notify_disable(MMB_CTX * mmb)
{
    int ret = 0;
    uint8_t value[2] = {0x00, 0x00};

    // Disable Sensor Data
    ret = mmb_gatt_send_char_hnd_write_req(mmb, MMB_PF_SENSOR_HND, value, 2);
    if (ret != 0)
        return ret;

    // Disable Sensor Notify
    ret = mmb_gatt_send_char_hnd_write_req(mmb, MMB_PF_SENSOR_NOTIFY, value, 2);
    if (ret != 0)
        return ret;

    return 0;
}

int mmb_handle_send_sensor_notify_enable(MMB_CTX * mmb)
{
    int ret = 0;
    uint8_t value[2] = {0x02, 0x00};

    // Enable Sensor Data
    ret = mmb_gatt_send_char_hnd_write_req(mmb, MMB_PF_SENSOR_HND, value, 2);
    if (ret != 0)
        return ret;

    // Endbale Sensor Notify
    return mmb_gatt_send_char_hnd_write_req(mmb, MMB_PF_SENSOR_NOTIFY, value, 2);
}

int mmb_handle_send_v_mode(MMB_CTX * mmb)
{
    uint8_t buf[1] = {2};
    return mmb_gatt_send_char_hnd_write_cmd(mmb, MMB_PF_VIBRATION_HND, buf, 1);
}

int mmb_handle_send_led_mode(MMB_CTX * mmb, enum mmb_led_mode_e mode)
{
    uint8_t buf[5] = {0,0,0,0,0};

    switch (mode)
    {
        case MMB_LED_RED_MODE:
            buf[0] = 14;
            buf[1] = 6;
            buf[2] = 1;
            buf[3] = 2;
            buf[4] = 1;
            break;

        case MMB_LED_BLUE_MODE:
            buf[0] = 14;
            buf[1] = 0;
            buf[2] = 6;
            buf[3] = 6;
            buf[4] = 1;
            break;

        case MMB_LED_ORANGE_MODE:
            buf[0] = 14;
            buf[1] = 6;
            buf[2] = 2;
            buf[3] = 0;
            buf[4] = 1;
            break;

        case MMB_LED_GREEN_MODE:
            buf[0] = 14;
            buf[1] = 4;
            buf[2] = 5;
            buf[3] = 0;
            buf[4] = 1;
            break;

        case MMB_LED_OFF_MODE:
        default:
            return -1;
            break;
    }

    return mmb_gatt_send_char_hnd_write_req(mmb, MMB_PF_CONTROL_HND, buf, 5);
}

