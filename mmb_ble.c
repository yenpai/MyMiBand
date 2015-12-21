#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

//#include "mmb_ctx.h"
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

#define ATT_CID 0x0004
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

    if (l2cap_bind(sock, src, BDADDR_LE_PUBLIC, 0, ATT_CID) < 0)
    {
        printf("l2cap_bind failed!\n");
        close(sock);
        return -3;
    }

    if (l2cap_connect(sock, dst, BDADDR_LE_PUBLIC, 0, ATT_CID) < 0)
    {
        printf("l2cap_connect failed!\n");
        close(sock);
        return -4;
    }

    return sock;

}

