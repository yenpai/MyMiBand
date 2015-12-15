#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>          // For fcntl
#include "mmb_util.h"

size_t bytes_to_hex_str(char * out, uint8_t * in, size_t len)
{
    size_t i = 0;
    char *p = out;

    for (i = 0 ; i < len ; i++)
        p += sprintf(p, "%02x", in[i]);

    *p = '\0';
    return (p - out);
}

size_t hex_str_split_to_bytes(uint8_t * out, size_t max, char * in, const char * split)
{
    char *s = NULL;
    size_t i = 0;

    s = strtok(in, split);

    while (s != NULL)
    {
        out[i] = 0xFF & strtoul(s, NULL, 16);
        s = strtok(NULL, split); 
        
        if (++i > max)
            break;
    }

    return i;
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

int socket_setting_non_blocking(int fd)
{
    int flags;

    // Get current flags
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;

    // Set non-block flags
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
        return -1;

    return 0;
}

