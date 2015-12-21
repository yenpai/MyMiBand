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

int mmb_miband_parsing_raw_data(MMB_CTX * mmb, uint8_t * buf, size_t size);

#endif /* ifndef MMB_MIBAND_H_ */

