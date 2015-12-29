#ifndef MMB_MIBAND_H_
#define MMB_MIBAND_H_

#include <stdint.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "evhr.h"
#include "eble.h"

#define MMB_MIBAND_TIMEOUT_SEC              30

#define MMB_BATTERY_STATUS_LOW              1
#define MMB_BATTERY_STATUS_CHARGING         2
#define MMB_BATTERY_STATUS_FULL             3
#define MMB_BATTERY_STATUS_NOT_CHARGING     4

struct mmb_battery_data_s {
    uint8_t     level;
    uint8_t     dt_year;
    uint8_t     dt_month;
    uint8_t     dt_day;
    uint8_t     dt_hour;
    uint8_t     dt_minute;
    uint8_t     dt_second;
    uint16_t    cycle;
    uint8_t     status;
} __attribute__((packed));

struct mmb_sensor_data_s {
    uint16_t seq;
    uint16_t x;
    uint16_t y;
    uint16_t z;
} __attribute__((packed));

struct mmb_realtime_data_s {
    uint32_t step;
} __attribute__((packed));

struct mmb_user_data_s {
    uint32_t uid; 
    uint8_t  gender; 
    uint8_t  age; 
    uint8_t  height;
    uint8_t  weight;
    uint8_t  type;
    uint8_t  alias[10];
    uint8_t  code;
} __attribute__((packed));

typedef struct mmb_miband_ctx_s {
    
    EBLE_DEVICE                 device;
    
    int                         status;
    struct mmb_user_data_s      user;
    struct mmb_battery_data_s   battery;
    struct mmb_sensor_data_s    sensor;
    struct mmb_realtime_data_s  realtime;

    struct evhr_event_s *       ev_ble;
    struct evhr_event_s *       ev_timeout;
    
    /* for mmb_miband_led */
    struct evhr_event_s *       ev_led_timer;
    int                         led_mode;
    int                         led_index;

} __attribute__((packed)) MMB_MIBAND;

/* mmb_miband.c */
int mmb_miband_probe(EBLE_DEVICE * device);
int mmb_miband_init(EBLE_DEVICE ** device);
int mmb_miband_start(MMB_MIBAND * this, EBLE_ADAPTER * adapter, EVHR_CTX * evhr);
int mmb_miband_stop(MMB_MIBAND * this);

/* mmb_miband_att.c */
int mmb_miband_send_auth(MMB_MIBAND * this);
int mmb_miband_send_realtime_notify(MMB_MIBAND * this, uint8_t enable);
int mmb_miband_send_sensor_notify(MMB_MIBAND * this, uint8_t enable);
int mmb_miband_send_battery_notify(MMB_MIBAND * this, uint8_t enable);

int mmb_miband_send_vibration(MMB_MIBAND * this, uint8_t mode);
int mmb_miband_send_ledcolor(MMB_MIBAND * this, uint32_t color);

int mmb_miband_send_battery_read(MMB_MIBAND * mmb);

/* LED COLOR = 0x(on/off)(B)(G)(R) */
#define MMB_LED_COLOR_OFF       0x01000000UL
#define MMB_LED_COLOR_RED       0x01000006UL
#define MMB_LED_COLOR_GREEN     0x01000600UL
#define MMB_LED_COLOR_BLUE      0x01060000UL
#define MMB_LED_COLOR_YELLOW    0x01000606UL
#define MMB_LED_COLOR_ORANGE    0x01000306UL
#define MMB_LED_COLOR_WHITE     0x01060606UL

#define MMB_LED_COLOR_LEVEL(COLOR, LEVEL) ( \
        (COLOR & 0xFF000000UL) | \
        (((COLOR >> 16 & 0xFF) * LEVEL / 6) << 16 ) | \
        (((COLOR >> 8  & 0xFF) * LEVEL / 6) << 8  ) | \
        (((COLOR       & 0xFF) * LEVEL / 6)       ) )

#define MMB_VIBRATION_STOP          0
#define MMB_VIBRATION_WITH_LED      1
#define MMB_VIBRATION_10_WITH_LED   2
#define MMB_VIBRATION_WITHOUT_LED   3

int mmb_miband_op_error(MMB_MIBAND * this, uint16_t hnd, uint8_t error_code);
int mmb_miband_op_read_type_resp(MMB_MIBAND * this, uint16_t hnd, uint8_t *val, size_t size);
int mmb_miband_op_read_resp(MMB_MIBAND * this, uint8_t *val, size_t size);
int mmb_miband_op_write_resp(MMB_MIBAND * this);
int mmb_miband_op_notification(MMB_MIBAND * this, uint16_t hnd, uint8_t *val, size_t size);

/* mmb_miband_led.c */
int mmb_miband_led_mode_change(MMB_MIBAND * this, int mode);
int mmb_miband_led_start(MMB_MIBAND * this, EVHR_CTX * evhr);
int mmb_miband_led_stop(MMB_MIBAND * this);

#endif /* ifndef MMB_MIBAND_H_ */

