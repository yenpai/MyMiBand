#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmb_ctx.h"
#include "mmb_util.h"
//#include "evhr.h"

extern int mmb_gatt_send_char_hnd_write_req(MMB_CTX * mmb, uint8_t hnd, uint8_t * data, size_t size);

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
            mmb, MIBAND_CHAR_HND_USERINFO, ptr, size);
}

int mmb_handle_send_sensor_notify_disable(MMB_CTX * mmb)
{
    int ret = 0;
    uint8_t value[2] = {0x00, 0x00};

    // Disable Sensor Data
    ret = mmb_gatt_send_char_hnd_write_req(mmb, MIBAND_CHAR_HND_SENSOR, value, 2);
    if (ret != 0)
        return ret;

    // Disable Sensor Notify
    ret = mmb_gatt_send_char_hnd_write_req(mmb, MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
    if (ret != 0)
        return ret;

    return 0;
}

int mmb_handle_send_sensor_notify_enable(MMB_CTX * mmb)
{
    int ret = 0;
    uint8_t value[2] = {0x02, 0x00};

    // Enable Sensor Data
    ret = mmb_gatt_send_char_hnd_write_req(mmb, MIBAND_CHAR_HND_SENSOR, value, 2);
    if (ret != 0)
        return ret;

    // Endbale Sensor Notify
    return mmb_gatt_send_char_hnd_write_req(mmb, MIBAND_CHAR_HND_SENSOR_NOTIFY, value, 2);
}

