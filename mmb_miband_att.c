#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "evhr.h"
#include "mmb_ctx.h"
#include "mmb_util.h"

/* BLE ATT OPCODE */
#define MMB_ATT_OPCODE_ERROR            0x01
#define MMB_ATT_OPCODE_READ_REQ         0x0A
#define MMB_ATT_OPCODE_READ_RESP        0x0B
#define MMB_ATT_OPCODE_WRITE_REQ        0x12
#define MMB_ATT_OPCODE_WRITE_RESP       0x13
#define MMB_ATT_OPCODE_WRITE_CMD        0x52
#define MMB_ATT_OPCODE_NOTIFICATION     0x1B
#define MMB_ATT_OPCODE_INDICATION       0x1D

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

int mmb_miband_parsing_notification(MMB_CTX * mmb, uint16_t hnd, uint8_t *val, size_t size)
{
    size_t i = 0;
    printf("[MMB][MIBAND][NOTIFICATION]");
    printf(" Hnd[0x%04x]\n\t", hnd);
    for (i=0;i<size;i++)
        printf("%02x ", val[i]);
    printf("\n");

    return 0;
}

int mmb_miband_parsing_raw_data(MMB_CTX * mmb, uint8_t * buf, size_t size)
{
    size_t i = 0;
    int ret = 0;

    switch (buf[0])
    {
        case MMB_ATT_OPCODE_ERROR:
            printf("[MMB][MIBAND][ERROR]");
            printf(" ReqOpcode[0x%02x]", buf[1]);
            printf(" Hnd[0x%02x%02x]", buf[3], buf[2]);
            printf(" ErrCode[0x%02x]", buf[4]);
            printf("\n");
            ret = -1;
            break;

        case MMB_ATT_OPCODE_READ_RESP:
            printf("[MMB][MIBAND][READ_RESP]\n\t");
            for (i=1;i<size;i++)
                printf("%02x ", buf[i]);
            printf("\n");
            break;

        case MMB_ATT_OPCODE_WRITE_RESP:
            printf("[MMB][MIBAND][WRITE_RESP]\n");
            break;

        case MMB_ATT_OPCODE_NOTIFICATION:
            ret = mmb_miband_parsing_notification(
                    mmb, (uint16_t) buf[1], buf + 3, size - 3);
            break;
    }

    return ret;
}

static int mmb_miband_write_req(int fd, uint16_t hnd, uint8_t *val, size_t size)
{

    int ret;
    uint8_t buf[MMB_BUFFER_SIZE];
    
    printf("[MMB][MIBAND][WriteReq] hnd=0x%04x size=%lu.\n", hnd, size);

    buf[0] = MMB_ATT_OPCODE_WRITE_REQ;
    buf[1] = 0xFF & hnd;
    buf[2] = 0xFF & hnd >> 8;
    memcpy(buf+3, val, size);

    ret = write(fd, buf, size + 3);
    if (ret < 0) {
        printf("[MMB][MIBAND][WriteReq][ERR] hnd=0x%04x failed!\n", hnd);
        return -errno;
    }
    
    return ret;
}

static int mmb_miband_write_cmd(int fd, uint16_t hnd, uint8_t *val, size_t size)
{

    int ret;
    uint8_t buf[MMB_BUFFER_SIZE];
    
    printf("[MMB][MIBAND][WriteCmd] hnd=0x%04x size=%lu.\n", hnd, size);

    buf[0] = MMB_ATT_OPCODE_WRITE_CMD;
    buf[1] = 0xFF & hnd;
    buf[2] = 0xFF & hnd >> 8;
    memcpy(buf+3, val, size);

    ret = write(fd, buf, size + 3);
    if (ret < 0) {
        printf("[MMB][MIBAND][WriteCmd][ERR] hnd = 0x%04x failed!\n", hnd);
        return -errno;
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

    ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_USER, ptr, size);
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
    ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_REALTIME, value, 2);
    if (ret < 0)
        return ret;

    // Disable Notify
    ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_REALTIME_NOTIFY, value, 2);
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
    ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_SENSOR, value, 2);
    if (ret < 0)
        return ret;

    // Disable Notify
    ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_SENSOR_NOTIFY, value, 2);
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

    ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_BATTERY, value, 2);
    if (ret < 0)
        return ret;

    ret = mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_BATTERY_NOTIFY, value, 2);
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
    return mmb_miband_write_cmd(mmb->ev_ble->fd, MMB_PF_HND_VIBRATION, buf, 1);
}

int mmb_miband_send_ledcolor(MMB_CTX * mmb, uint32_t color)
{
    uint8_t buf[5] = {14,0,0,0,0};

    buf[1] = 0xFF & color      ; // R
    buf[2] = 0xFF & color >> 8 ; // G
    buf[3] = 0xFF & color >> 16; // B
    buf[4] = 0xFF & color >> 24; // onoff

    printf("%08x\n", color);
    printf("%02x %02x %02x %02x\n", buf[1], buf[2], buf[3], buf[4]);
#if 0
    switch (mode)
    {
        case MMB_LED_COLOR_RED:
            buf[1] = 6;
            buf[2] = 0;
            buf[3] = 0;
            buf[4] = 1;
            break;

        case MMB_LED_COLOR_GREEN:
            buf[1] = 0;
            buf[2] = 6;
            buf[3] = 0;
            buf[4] = 1;
            break;

        case MMB_LED_COLOR_BLUE:
            buf[0] = 14;
            buf[1] = 0;
            buf[2] = 0;
            buf[3] = 6;
            buf[4] = 1;
            break;

        case MMB_LED_COLOR_YELLOW:
            buf[0] = 14;
            buf[1] = 6;
            buf[2] = 6;
            buf[3] = 0;
            buf[4] = 1;
            break;

        case MMB_LED_COLOR_ORANGE:
            buf[0] = 14;
            buf[1] = 6;
            buf[2] = 3;
            buf[3] = 0;
            buf[4] = 1;
            break;

        case MMB_LED_COLOR_WHITE:
            buf[0] = 14;
            buf[1] = 6;
            buf[2] = 6;
            buf[3] = 6;
            buf[4] = 1;
            break;

        case MMB_LED_COLOR_OFF:
            buf[0] = 14;
            buf[1] = 0;
            buf[2] = 0;
            buf[3] = 0;
            buf[4] = 1;
            break;

        default:
            return -1;
            break;
    }
#endif

    return mmb_miband_write_req(mmb->ev_ble->fd, MMB_PF_HND_CONTROL, buf, 5);
}

