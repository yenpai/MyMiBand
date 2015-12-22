#ifndef MMB_CTX_H_
#define MMB_CTX_H_

//#include <stdint.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define MMB_MIBAND_TIMEOUT_SEC      10
#define MMB_BUFFER_SIZE             512

#define MMB_BATTERY_STATUS_LOW          1
#define MMB_BATTERY_STATUS_CHARGING     2
#define MMB_BATTERY_STATUS_FULL         3
#define MMB_BATTERY_STATUS_NOT_CHARGING 4

struct mmb_batteery_data_s {
    uint8_t     level;
    uint8_t     dt_year;
    uint8_t     dt_month;
    uint8_t     dt_day;
    uint8_t     dt_hour;
    uint8_t     dt_minute;
    uint8_t     dt_second;
    uint16_t    cycle;
    uint8_t     status;
};

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
    bdaddr_t                    addr;
    struct mmb_user_data_s      user;
    struct mmb_batteery_data_s  battery;
    struct mmb_sensor_data_s    sensor;
    struct mmb_sensor_data_s    sensor_old;
};

enum mmb_status_e {
    MMB_STATUS_STOPPED      = -1,
    MMB_STATUS_INITIAL      = 0,
    MMB_STATUS_CONNECTING   = 1,
    MMB_STATUS_CONNECTED    = 2,
};

typedef struct mmb_ctx_s {
    enum mmb_status_e       status;
    bdaddr_t                addr;
    struct evhr_ctx_s *     evhr;
    struct evhr_event_s *   ev_ble;
    struct evhr_event_s *   ev_timeout;
    struct mmb_data_s       data;
} MMB_CTX;

#endif
