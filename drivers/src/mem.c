#include <stddef.h>

/* GCC emits calls to these on its own, for `= {0}` and struct copies, even
   with no C library linked. Firmware only: host builds use the real libc. */

void *memset(void *dst, int value, size_t n){
    unsigned char *d = dst;
    while (n--) {
        *d++ = (unsigned char)value;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n){
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}
