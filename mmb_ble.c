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
        printf("l2cap_bind failed! errno[%d]\n", errno);
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
            printf("l2cap_connect failed! errno[%d]\n", errno);
            return -1;
        }
    }

    return 0;
}

int mmb_ble_scan_reader(const int dev)
{
    uint8_t buf[HCI_MAX_EVENT_SIZE];
    int len;
    evt_le_meta_event * meta_event;
    le_advertising_info * info;
    uint8_t reports_count;
    void * offset;
    char addr[18];

    len = read(dev, buf, sizeof(buf));
    if ( len >= HCI_EVENT_HDR_SIZE ) {
        meta_event = (evt_le_meta_event*)(buf+HCI_EVENT_HDR_SIZE+1);
        if ( meta_event->subevent == EVT_LE_ADVERTISING_REPORT ) {
            reports_count = meta_event->data[0];
            offset = meta_event->data + 1;
            while ( reports_count-- ) {
                info = (le_advertising_info *) offset;
                ba2str(&(info->bdaddr), addr);
                printf("%s - RSSI %d\n", addr, (uint8_t)info->data[info->length]);
                offset = info->data + info->length + 2;
            }
        }
    }

    return 0;
}

int mmb_ble_scan_stop(const int dev, const int timeout)
{
    struct hci_request        scan_enable_rq;
    le_set_scan_enable_cp     scan_enable_cp;
    
    int status, ret;

    memset(&scan_enable_cp, 0, sizeof(scan_enable_cp));
    scan_enable_cp.enable = 0x00; // Disable flag.

    memset(&scan_enable_rq, 0, sizeof(scan_enable_rq));
    scan_enable_rq.ogf      = OGF_LE_CTL;
    scan_enable_rq.ocf      = OCF_LE_SET_SCAN_ENABLE; 
    scan_enable_rq.cparam   = &scan_enable_cp;
    scan_enable_rq.clen     = LE_SET_SCAN_ENABLE_CP_SIZE;
    scan_enable_rq.rparam   = &status;
    scan_enable_rq.rlen     = 1;
    
    if ((ret = hci_send_req(dev, &scan_enable_rq, timeout)) < 0)
        return -1;

    return 0;
}

int mmb_ble_scan_start(const int dev, const int timeout)
{
    struct hci_request        scan_para_rq;
    le_set_scan_parameters_cp scan_para_cp;

    struct hci_request        event_mask_rq;
    le_set_event_mask_cp      event_mask_cp;

    struct hci_request        scan_enable_rq;
    le_set_scan_enable_cp     scan_enable_cp;

    struct hci_filter nf;
    int status, ret, i;

    //socket_setting_non_blocking(dev);

    // 1. Set BLE scan parameters
    memset(&scan_para_cp, 0, sizeof(scan_para_cp));
    scan_para_cp.type             = 0x00;
    scan_para_cp.interval         = htobs(0x0010);
    scan_para_cp.window           = htobs(0x0010);
    scan_para_cp.own_bdaddr_type  = 0x00; // Public Device Address (default).
    scan_para_cp.filter           = 0x00; // Accept all.

    memset(&scan_para_rq, 0, sizeof(scan_para_rq));
    scan_para_rq.ogf      = OGF_LE_CTL;
    scan_para_rq.ocf      = OCF_LE_SET_SCAN_PARAMETERS;
    scan_para_rq.cparam   = &scan_para_cp;
    scan_para_rq.clen     = LE_SET_SCAN_PARAMETERS_CP_SIZE;
    scan_para_rq.rparam   = &status;
    scan_para_rq.rlen     = 1;

    if ((ret = hci_send_req(dev, &scan_para_rq, timeout)) < 0)
        return -1;

    // 2. Set BLE events report mask
    memset(&event_mask_cp, 0, sizeof(event_mask_cp));
    for(i = 0 ; i < 8 ; i++)
        event_mask_cp.mask[i] = 0xFF;
    
    memset(&event_mask_rq, 0, sizeof(event_mask_rq));
    event_mask_rq.ogf      = OGF_LE_CTL;
    event_mask_rq.ocf      = OCF_LE_SET_EVENT_MASK; 
    event_mask_rq.cparam   = &event_mask_cp;
    event_mask_rq.clen     = LE_SET_EVENT_MASK_CP_SIZE;
    event_mask_rq.rparam   = &status;
    event_mask_rq.rlen     = 1;

    if ((ret = hci_send_req(dev, &event_mask_rq, timeout)) < 0)
        return -2;

    // 3. Set BLE sacn enable
    memset(&scan_enable_cp, 0, sizeof(scan_enable_cp));
    scan_enable_cp.enable      = 0x01; // Enable flag.
    scan_enable_cp.filter_dup  = 0x00; // Filtering disabled.
    
    memset(&scan_enable_rq, 0, sizeof(scan_enable_rq));
    scan_enable_rq.ogf      = OGF_LE_CTL;
    scan_enable_rq.ocf      = OCF_LE_SET_SCAN_ENABLE; 
    scan_enable_rq.cparam   = &scan_enable_cp;
    scan_enable_rq.clen     = LE_SET_SCAN_ENABLE_CP_SIZE;
    scan_enable_rq.rparam   = &status;
    scan_enable_rq.rlen     = 1;

    if ((ret = hci_send_req(dev, &scan_enable_rq, timeout)) < 0)
        return -3;

    // 4. Set hci filter
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);
    if ( setsockopt(dev, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0 )
        return -4;

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
