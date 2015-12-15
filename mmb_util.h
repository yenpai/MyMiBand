#ifndef MMB_UTIL_H
#define MMB_UTIL_H

#include <stdio.h>
#include <stdint.h>


/*
 * unused attribute macros
 * example:
 *   void foo(int UNUSED(bar)) { ... }
 *   static void UNUSED_FUNCTION(foo)(int bar) { ... }
 */
#ifdef __GNUC__
    #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
    #define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
    #define UNUSED(x) UNUSED_ ## x
    #define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

size_t bytes_to_hex_str(char * out, uint8_t * in, size_t size);
size_t hex_str_split_to_bytes(uint8_t * out, size_t max, char * in, const char * split);

uint8_t crc8(uint8_t crc, uint8_t *data, size_t len);
int socket_setting_non_blocking(int fd);
int socket_udp_send_broadcast(unsigned short port, uint8_t * buf, size_t size);

#endif
