#ifndef MMB_CTX_H_
#define MMB_CTX_H_

#include <stdint.h>

struct mmb_user_info_s {
    union {
        uint8_t data[20];
        struct {
            uint32_t uid; 
            uint8_t  gender; 
            uint8_t  age; 
            uint8_t  height;
            uint8_t  weight;
            uint8_t  type;
            uint8_t  alias[10];
            uint8_t  code;
        };
    };
};

#define CMD_BUFFER_SIZE     512
#define CMD_GATTTOOL_PATH   "/usr/bin/gatttool"

#define RESPONSE_NOTIFICATION_STR "Notification"

#define MIBAND_CHAR_HND_USERINFO        0x0019
#define MIBAND_CHAR_HND_SENSOR_DATA     0x0031
#define MIBAND_CHAR_HND_SENSOR_NOTIFY   0x0032

typedef struct mmb_ctx_s {
    char hci_dev[6];
    char miband_mac[18];
    struct mmb_user_info_s user_info;
} MMB_CTX;

#endif
