#ifndef MMB_UTIL_H
#define MMB_UTIL_H

#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t b[6];
} bdaddr_t;

int bytes_to_hex_str(char * out, uint8_t * in, size_t size);
uint8_t crc8(uint8_t crc, uint8_t *data, size_t len);

#endif
