#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>          // For fcntl

#include <arpa/inet.h>
#include <sys/socket.h>

#include "mmb_util.h"

void dump_hex_bytes(char * title, uint8_t * buf, size_t size)
{
    size_t i = 0;

    printf("== DUMP [%s] HEX BYTES ==", title);
    for (i = 0 ; i < size ; i++)
    {
        if (i % 16 == 0)
            printf("\n\t");
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

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

int socket_udp_send_broadcast(unsigned short port, uint8_t * buf, size_t size)
{
    int sockfd = -1;
    int optval = 1;
    struct sockaddr_in dst_addr;

    if ((sockfd = socket(AF_INET,SOCK_DGRAM,0) == -1))
        return -1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, 
                &optval, sizeof(optval)) == -1)
    {
        close(sockfd);
        return -2;
    }

    dst_addr.sin_family         = AF_INET; 
    dst_addr.sin_port           = htons(port); 
    dst_addr.sin_addr.s_addr    = INADDR_BROADCAST; 

    if (sendto(sockfd, buf, size, 0, 
            (struct sockaddr *) &dst_addr, sizeof(dst_addr)) == -1)
    {
        close(sockfd);
        return -3;
    }

    close(sockfd);

    return 0;
}

