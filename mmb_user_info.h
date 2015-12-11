#ifndef MMB_USER_INFO_H
#define MMB_USER_INFO_H

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

void mmb_user_info_gen_code(struct mmb_user_info_s * user_info, uint8_t mac);
void mmb_user_info_get_hex_str(struct mmb_user_info_s * user_info, char *buf);

#endif
