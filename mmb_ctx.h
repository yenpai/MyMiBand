#ifndef MMB_CTX_H_
#define MMB_CTX_H_

//#include <stdint.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define MMB_MIBAND_TIMEOUT_SEC      10
#define MMB_BUFFER_SIZE             512

/* PROFILE UUID */
#define MMB_PF_USER_UUID        0xff04
#define MMB_PF_CONTROL_UUID     0xff05
#define MMB_PF_REALTIME_UUID    0xff06
#define MMB_PF_LEPARAM_UUID     0xff09
#define MMB_PF_BATTERY_UUID     0xff0c
#define MMB_PF_TEST_UUID        0xff0d
#define MMB_PF_SENSOR_UUID      0xff0e
#define MMB_PF_PAIR_UUID        0xff0f
#define MMB_PF_VIBRATION_UUID   0x2a06

/* PROFILE Handle */
#define MMB_PF_USER_HND         0x0019
#define MMB_PF_CONTROL_HND      0x001b
#define MMB_PF_REALTIME_HND     0x001d
#define MMB_PF_REALTIME_NOTIFY  0x001e
#define MMB_PF_LEPARAM_HND      0x0025
#define MMB_PF_LEPARAM_NOTIFY   0x0026
#define MMB_PF_BATTERY_HND      0x002c
#define MMB_PF_BATTERY_NOTIFY   0x002d
#define MMB_PF_TEST_HND         0x002f
#define MMB_PF_SENSOR_HND       0x0031
#define MMB_PF_SENSOR_NOTIFY    0x0032
#define MMB_PF_PAIR_HND         0x0034
#define MMB_PF_VIBRATION_HND    0x0051


enum mmb_led_mode_e {
    MMB_LED_OFF_MODE,
    MMB_LED_RED_MODE,
    MMB_LED_BLUE_MODE,
    MMB_LED_ORANGE_MODE,
    MMB_LED_GREEN_MODE,
};

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

typedef struct mmb_ctx_s {
    uint8_t                 status;
    bdaddr_t                addr;
    struct evhr_ctx_s *     evhr;
    struct evhr_event_s *   ev_ble;
    struct evhr_event_s *   ev_timeout;
    int timerfd;
    struct mmb_data_s       data;
} MMB_CTX;

#endif
