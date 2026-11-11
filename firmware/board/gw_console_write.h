#pragma once
#include <stddef.h>
/* A record may exceed the USB TX ring (256 bytes). Send bounded chunks;
 * stop on a timeout/error instead of spinning forever or skipping bytes. */
static inline int gw_console_write(const char* s, size_t size,
                                  int (*send)(const char*, size_t)) {
    while (size) {
        size_t n=size>128?128:size;
        int sent=send(s,n);
        if (sent<=0 || (size_t)sent>n) return 0;
        s+=sent; size-=(size_t)sent;
    }
    return 1;
}
