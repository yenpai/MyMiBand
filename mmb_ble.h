#ifndef MMB_BLE_H_
#define MMB_BLE_H_

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#define MMB_BLE_ATT_CID                     0x0004
#define MMB_BLE_ATT_OPCODE_ERROR            0x01
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
int mmb_ble_write_req(int fd, uint16_t hnd, uint8_t *val, size_t size);
int mmb_ble_write_cmd(int fd, uint16_t hnd, uint8_t *val, size_t size);

#endif 
