#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "mmb_ble.h"
#include "mmb_util.h"

static int l2cap_socket_create()
{
    // Connection-oriented (SOCK_SEQPACKET)
    // Connectionless (SOCK_DGRAM)
    return socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
}

static int l2cap_bind(int sock, const bdaddr_t *src, uint8_t src_type, uint16_t psm, uint16_t cid)
{
    struct sockaddr_l2 addr;
    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_bdaddr_type = src_type;
    bacpy(&addr.l2_bdaddr, src);

    if (cid)
        addr.l2_cid = htobs(cid);
    else
        addr.l2_psm = htobs(psm);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        printf("l2cap_bind failed! errno[%d]", errno);
        return -1;
    }

    return 0;
}

static int l2cap_connect(int sock, const bdaddr_t *dst, uint8_t dst_type, uint16_t psm, uint16_t cid)
{
    struct sockaddr_l2 addr;

    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_bdaddr_type = dst_type;
    bacpy(&addr.l2_bdaddr, dst);
    if (cid)
        addr.l2_cid = htobs(cid);
    else
        addr.l2_psm = htobs(psm);

    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        if (!(errno == EAGAIN || errno == EINPROGRESS))
        {
            printf("l2cap_connect failed! errno[%d]", errno);
            return -1;
        }
    }

    return 0;
}

int mmb_ble_connect(const bdaddr_t * src, const bdaddr_t * dst)
{

    int sock;

    sock = l2cap_socket_create();
    if (sock < 0)
    {
        printf("l2cap_socket_create failed!\n");
        return -1;
    }

    if (socket_setting_non_blocking(sock) < 0)
    {
        printf("socket_setting_non_blocking failed!\n");
        close(sock);
        return -2;
    }
    
    if (l2cap_bind(sock, src, BDADDR_LE_PUBLIC, 0, MMB_BLE_ATT_CID) < 0)
    {
        printf("l2cap_bind failed!\n");
        close(sock);
        return -3;
    }

    if (l2cap_connect(sock, dst, BDADDR_LE_PUBLIC, 0, MMB_BLE_ATT_CID) < 0)
    {
        printf("l2cap_connect failed!\n");
        close(sock);
        return -4;
    }

    return sock;

}

int mmb_ble_read_type_req(int fd, uint16_t hnd, uint16_t uuid)
{
    int ret;
    uint8_t buf[7];
    
    buf[0] = MMB_BLE_ATT_OPCODE_READ_TYPE_REQ;
    buf[1] = 0xFF & hnd;
    buf[2] = 0xFF & hnd >> 8;
    buf[3] = 0xFF & hnd;
    buf[4] = 0xFF & hnd >> 8;
    buf[5] = 0xFF & uuid;
    buf[6] = 0xFF & uuid >> 8;
 
    ret = write(fd, buf, 7);
    if (ret < 0) {
        printf("[MMB][BLE][ReadTYpeReq][ERR] hnd = 0x%04x failed!\n", hnd);
        return -errno;
    }

    return ret;
}

int mmb_ble_read_req(int fd, uint16_t hnd)
{
    int ret;
    uint8_t buf[3];
    
    buf[0] = MMB_BLE_ATT_OPCODE_READ_REQ;
    buf[1] = 0xFF & hnd;
    buf[2] = 0xFF & hnd >> 8;
 
    ret = write(fd, buf, 3);
    if (ret < 0) {
        printf("[MMB][BLE][ReadReq][ERR] hnd = 0x%04x failed!\n", hnd);
        return -errno;
    }

    return ret;
}

int mmb_ble_write_req(int fd, uint16_t hnd, uint8_t *val, size_t size)
{
    int ret;
    uint8_t buf[MMB_BLE_BUFFER_MAX];
    
    buf[0] = MMB_BLE_ATT_OPCODE_WRITE_REQ;
    buf[1] = 0xFF & hnd;
    buf[2] = 0xFF & hnd >> 8;
    memcpy(buf+3, val, size);

    ret = write(fd, buf, size + 3);
    if (ret < 0) {
        printf("[MMB][BLE][WriteReq][ERR] hnd=0x%04x failed!\n", hnd);
        return -errno;
    }
    
    return ret;
}

int mmb_ble_write_cmd(int fd, uint16_t hnd, uint8_t *val, size_t size)
{
    int ret;
    uint8_t buf[MMB_BLE_BUFFER_MAX];

    buf[0] = MMB_BLE_ATT_OPCODE_WRITE_CMD;
    buf[1] = 0xFF & hnd;
    buf[2] = 0xFF & hnd >> 8;
    memcpy(buf+3, val, size);

    ret = write(fd, buf, size + 3);
    if (ret < 0) {
        printf("[MMB][BLE][WriteCmd][ERR] hnd = 0x%04x failed!\n", hnd);
        return -errno;
    }
    
    return ret;
}

int mmb_ble_att_data_parser(uint8_t *buf, size_t size, struct mmb_ble_att_data_parser_cb_s * cb, void *pdata)
{
    int ret = 0;

    switch (buf[0])
    {
        case MMB_BLE_ATT_OPCODE_ERROR:
            if (cb && cb->error_cb)
                ret = cb->error_cb( 
                        pdata, 
                        (uint16_t) buf[2],
                        buf[4]);
            else
                ret = -1002;
            break;

        case MMB_BLE_ATT_OPCODE_READ_TYPE_RESP:
            if (cb && cb->read_type_resp_cb)
                ret = cb->read_type_resp_cb( 
                        pdata, 
                        (uint16_t) buf[2],
                        buf + 4,
                        buf[1]);
            else
                ret = -1002;
            break;
        
        case MMB_BLE_ATT_OPCODE_READ_RESP:
            if (cb && cb->read_resp_cb)
                ret = cb->read_resp_cb( 
                        pdata, 
                        buf + 1,
                        size - 1);
            else
                ret = -1002;
            break;

        case MMB_BLE_ATT_OPCODE_WRITE_RESP:
            if (cb && cb->write_resp_cb)
                ret = cb->write_resp_cb(pdata); 
            else
                ret = -1002;
            break;

        case MMB_BLE_ATT_OPCODE_NOTIFICATION:
            if (cb && cb->notify_cb)
                ret = cb->notify_cb( 
                        pdata, 
                        (uint16_t) buf[1],
                        buf + 3,
                        size - 3);
            else
                ret = -1002;
            break;
        default:
            ret = -1001;
            break;
    }

    return ret;
}
