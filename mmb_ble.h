#ifndef MMB_BLE_H_
#define MMB_BLE_H_

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "qlist.h"

#define MMB_BLE_ATT_CID                     0x0004
#define MMB_BLE_ATT_OPCODE_ERROR            0x01
#define MMB_BLE_ATT_OPCODE_READ_TYPE_REQ    0x08
#define MMB_BLE_ATT_OPCODE_READ_TYPE_RESP   0x09
#define MMB_BLE_ATT_OPCODE_READ_REQ         0x0A
#define MMB_BLE_ATT_OPCODE_READ_RESP        0x0B
#define MMB_BLE_ATT_OPCODE_WRITE_REQ        0x12
#define MMB_BLE_ATT_OPCODE_WRITE_RESP       0x13
#define MMB_BLE_ATT_OPCODE_WRITE_CMD        0x52
#define MMB_BLE_ATT_OPCODE_NOTIFICATION     0x1B
#define MMB_BLE_ATT_OPCODE_INDICATION       0x1D

#define MMB_BLE_BUFFER_MAX                  512

int mmb_ble_connect(const bdaddr_t * src, const bdaddr_t * dst);
int mmb_ble_read_req(int fd, uint16_t hnd);
int mmb_ble_read_type_req(int fd, uint16_t hnd, uint16_t uuid);
int mmb_ble_write_req(int fd, uint16_t hnd, uint8_t *val, size_t size);
int mmb_ble_write_cmd(int fd, uint16_t hnd, uint8_t *val, size_t size);

struct mmb_ble_att_data_parser_cb_s {
    int (*error_cb) (void *pdata, uint16_t hnd, uint8_t error_code); 
    int (*read_type_resp_cb) (void *pdata, uint16_t hnd, uint8_t *val, size_t size); 
    int (*read_resp_cb) (void *pdata, uint8_t *val, size_t size); 
    int (*write_resp_cb) (void *pdata); 
    int (*notify_cb) (void *pdata, uint16_t hnd, uint8_t *val, size_t size); 
};

int mmb_ble_att_data_parser(uint8_t *buf, size_t size, struct mmb_ble_att_data_parser_cb_s * cb, void *pdata);

struct mmb_ble_advertising_s {
    bdaddr_t addr;
    uint8_t  rssi;
    char     name[32];
} __attribute__((packed));

int mmb_ble_scan_start(const int dd);
int mmb_ble_scan_stop(const int dd);
int mmb_ble_scan_reader(const int dd, QList * qlist);

#endif 
