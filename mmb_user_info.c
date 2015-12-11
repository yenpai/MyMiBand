#include <stdio.h>
#include <string.h>

#include "mmb_user_info.h"

static uint8_t crc8(uint8_t crc, uint8_t *data, size_t len)
{
    int i = 0, j = 0;
    uint8_t extract, sum;
    
    while (len-- > 0)
    {
        extract = data[i++];
        for (j = 8; j != 0 ; j--)
        {
            sum = (crc & 0xff) ^ (extract & 0xff);
            sum = (sum & 0xff) & 0x01;
            crc = (crc & 0xff) >> 1;

            if (sum != 0)
                crc = (crc & 0xff) ^ 0x8c;

            extract = (extract & 0xff) >> 1;
        }
    }

    return (crc & 0xff);
}

void mmb_user_info_gen_code(struct mmb_user_info_s * user_info, uint8_t mac)
{
    user_info->code = crc8(0x00, (uint8_t *) user_info, sizeof(struct mmb_user_info_s) - 1) ^ mac;
}

void mmb_user_info_get_hex_str(struct mmb_user_info_s * user_info, char * buf)
{
    int i;
    int len = sizeof (struct mmb_user_info_s);
    char *p = buf;
    for (i=0;i<len;i++)
        p += sprintf(p, "%02x", user_info->data[i]);
}

#if 0
int main(void)
{
    int i = 0;
    struct miband_user_info_s user_info;
    unsigned char *p = (unsigned char *) &user_info;

    memset(&user_info, 0, sizeof(struct miband_user_info_s));
    user_info.uid       = 19820610;
    user_info.gender    = 1;
    user_info.age       = 32;
    user_info.height    = 175;
    user_info.weight    = 60;
    user_info.type      = 0;
    strncpy((char *)user_info.alias, "RobinMI", sizeof(user_info.alias));
    
    miband_user_info_cal_check_code(&user_info, 0x08);

    for (i = 0; i < (int) sizeof(struct miband_user_info_s); i++)
        printf("%02x", p[i]);

    printf("\n");
       
    strncpy((char *)user_info.alias, "RobinMI2", sizeof(user_info.alias));
    
    miband_user_info_cal_check_code(&user_info, 0x08);

    for (i = 0; i < (int) sizeof(struct miband_user_info_s); i++)
        printf("%02x", p[i]);

    printf("\n");
    return 0;
}
#endif
