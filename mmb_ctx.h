#ifndef MMB_CTX_H_
#define MMB_CTX_H_

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#define CMD_BUFFER_SIZE     512
#define CMD_GATTTOOL_PATH   "/usr/bin/gatttool"

#define RESPONSE_NOTIFICATION_STR "Notification"

#define MIBAND_CHAR_HND_USERINFO        0x0019
#define MIBAND_CHAR_HND_SENSOR          0x0031
#define MIBAND_CHAR_HND_SENSOR_NOTIFY   0x0032

#define MMB_GATT_LISTEN_TIMEOUT_SEC     10

struct mmb_user_data_s {
    uint32_t uid; 
    uint8_t  gender; 
    uint8_t  age; 
    uint8_t  height;
    uint8_t  weight;
    uint8_t  type;
    uint8_t  alias[10];
    uint8_t  code;
};

struct mmb_sensor_data_s {
    uint16_t seq;
    uint16_t x;
    uint16_t y;
    uint16_t z;
};

struct mmb_data_s {
    char                        miband_mac[18];
    struct mmb_user_data_s      user;
    struct mmb_sensor_data_s    sensor;
    struct mmb_sensor_data_s    sensor_old;
};

struct mmb_gatt_s {
    int     is_running;
    FILE *  popen_fd;
    struct  evhr_event_s * time_ev;
    struct  evhr_event_s * proc_ev;
    char    buf[CMD_BUFFER_SIZE * 2];
    size_t  buf_size;
    void *  pdata;
};

typedef struct mmb_ctx_s {
    char hci_dev[6];
    struct evhr_ctx_s * evhr;
    struct mmb_gatt_s gatt;
    struct mmb_data_s data;
} MMB_CTX;

#endif
