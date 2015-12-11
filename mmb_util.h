#ifndef MMB_UTIL_H
#define MMB_UTIL_H

#include <stdio.h>
#include <stdint.h>

int bytes_to_hex_str(char * out, uint8_t * in, size_t size);
uint8_t crc8(uint8_t crc, uint8_t *data, size_t len);
int socket_setting_non_blocking(int fd);

#endif
