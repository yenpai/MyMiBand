#ifndef MMB_MIBAND_H_
#define MMB_MIBAND_H_

#include "mmb_ctx.h"

/* mmb_miband.c */
int mmb_miband_start(MMB_CTX * mmb);
int mmb_miband_stop(MMB_CTX * mmb);

/* mmb_miband_att.c */
int mmb_miband_send_auth(MMB_CTX * mmb);
int mmb_miband_send_realtime_notify(MMB_CTX * mmb, uint8_t enable);
int mmb_miband_send_sensor_notify(MMB_CTX * mmb, uint8_t enable);
int mmb_miband_send_battery_notify(MMB_CTX * mmb, uint8_t enable);

int mmb_miband_send_vibration(MMB_CTX * mmb, uint8_t mode);
int mmb_miband_send_ledcolor(MMB_CTX * mmb, uint32_t color);

int mmb_miband_parsing_raw_data(MMB_CTX * mmb, uint8_t * buf, size_t size);

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

#endif /* ifndef MMB_MIBAND_H_ */

