#include <stdio.h>
#include <stdlib.h>
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

#define EIR_FLAGS                   0x01  /* flags */
#define EIR_UUID16_SOME             0x02  /* 16-bit UUID, more available */
#define EIR_UUID16_ALL              0x03  /* 16-bit UUID, all listed */
#define EIR_UUID32_SOME             0x04  /* 32-bit UUID, more available */
#define EIR_UUID32_ALL              0x05  /* 32-bit UUID, all listed */
#define EIR_UUID128_SOME            0x06  /* 128-bit UUID, more available */
#define EIR_UUID128_ALL             0x07  /* 128-bit UUID, all listed */
#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */
#define EIR_TX_POWER                0x0A  /* transmit power level */
#define EIR_DEVICE_ID               0x10  /* device ID */

static void eir_parse_name(uint8_t *eir, size_t eir_len, char *buf, size_t buf_len)
{
	size_t offset = 0;

	while (offset < eir_len) {

		uint8_t field_len = eir[0];
		size_t name_len;

		/* Check for the end of EIR */
		if (field_len == 0)
			break;

		if (offset + field_len > eir_len)
			break;

		switch (eir[1]) {
		case EIR_NAME_SHORT:
		case EIR_NAME_COMPLETE:
			name_len = field_len - 1;
			if (name_len >= buf_len)
            {
                memcpy(buf, &eir[2], buf_len - 1);
                buf[buf_len] = '\0';
                return;
            }

			memcpy(buf, &eir[2], name_len);
            buf[name_len] = '\0';
			return;
		}

		offset += field_len + 1;
		eir += field_len + 1;
	}
    
    buf[0] = '\0';
}

int mmb_ble_scan_reader(const int dd, QList * qlist)
{
    uint8_t buf[HCI_MAX_EVENT_SIZE];
    int len;
    evt_le_meta_event * meta_event;
    le_advertising_info * info;
    uint8_t count;
    void * offset;

    struct mmb_ble_advertising_s * adv = NULL;
    int adv_count = 0;

    while (1)
    {

        len = read(dd, buf, sizeof(buf));
        if (len < HCI_EVENT_HDR_SIZE)
            break;

        meta_event = (evt_le_meta_event*)(buf+HCI_EVENT_HDR_SIZE+1);
        if (meta_event->subevent != EVT_LE_ADVERTISING_REPORT)
            continue;

        count = meta_event->data[0];
        offset = meta_event->data + 1;

        while (count--) {

            // Got info
            info = (le_advertising_info *) offset;

            // Parsing Data
            adv = malloc(sizeof(struct mmb_ble_advertising_s));
            if (adv == NULL)
                break;;

            memset(adv, 0, sizeof(struct mmb_ble_advertising_s));
            bacpy(&adv->addr, &info->bdaddr);
            adv->rssi = (uint8_t)info->data[info->length];
            eir_parse_name(info->data, info->length,
                    adv->name, sizeof(adv->name));

            // Put device into qlist_new
            qlist_push(qlist, adv);

            // Next offset
            offset = info->data + info->length + 2;

        }

    }

    return adv_count;
}

int mmb_ble_scan_stop(const int dd)
{
#if 0 
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
    
    if ((ret = hci_send_req(dd, &scan_enable_rq, 10000)) < 0)
        return -1;
#else
    int ret;
	uint8_t filter_dup = 0x01;

	ret = hci_le_set_scan_enable(dd, 0x00, filter_dup, 10000);
	if (ret < 0) {
		perror("Disable scan failed");
        return -1;
	}
#endif
    return 0;
}

int mmb_ble_scan_start(const int dd)
{
#if 0
    struct hci_request        scan_para_rq;
    le_set_scan_parameters_cp scan_para_cp;

    struct hci_request        event_mask_rq;
    le_set_event_mask_cp      event_mask_cp;

    struct hci_request        scan_enable_rq;
    le_set_scan_enable_cp     scan_enable_cp;

    struct hci_filter nf;
    int status, ret, i;

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

    if ((ret = hci_send_req(dd, &scan_para_rq, 10000)) < 0)
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

    if ((ret = hci_send_req(dd, &event_mask_rq, 10000)) < 0)
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

    if ((ret = hci_send_req(dd, &scan_enable_rq, 10000)) < 0)
        return -3;
#else

    struct hci_filter nf;

	int ret;
	uint8_t own_type = LE_PUBLIC_ADDRESS;
	uint8_t scan_type = 0x01;
	uint8_t filter_policy = 0x00;
	uint16_t interval = htobs(0x0010);
	uint16_t window = htobs(0x0010);
	uint8_t filter_dup = 0x01;

	ret = hci_le_set_scan_parameters(dd, scan_type, interval, window,
						own_type, filter_policy, 10000);
	if (ret < 0) {
		perror("Set scan parameters failed");
        return -2;
	}

	ret = hci_le_set_scan_enable(dd, 0x01, filter_dup, 10000);
	if (ret < 0) {
		perror("Enable scan failed");
        return -3;
	}
#endif

    // 4. Set hci filter
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);
    if ( setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0 )
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
