#include <stdio.h>
#include "mmb_util.h"

int bytes_to_hex_str(char * out, uint8_t * in, size_t len)
{
    size_t i;
    char *p = out;
    for (i=0;i<len;i++)
        p += sprintf(p, "%02x", in[i]);
    *p = '\0';
    return (p - out);
}

uint8_t crc8(uint8_t crc, uint8_t *data, size_t len)
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
