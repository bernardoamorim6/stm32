#include "../include/formatter.h"

/* Digits before the point only: INT32_MIN gives 21474836, eight of them */
#define RAW_MEASUREMENT_SIZE 8

uint8_t measurement_formatter(int32_t value, char *s){
    char digits[RAW_MEASUREMENT_SIZE];
    uint8_t cur = 0;
    if (value < 0) {
        s[cur] = '-';
        cur++;
    }
    /* Negated as unsigned, because -INT32_MIN does not fit in an int32_t */
    uint32_t n = (value < 0) ? (0U - (uint32_t)value) : (uint32_t)value;
    uint32_t whole = n / 100;
    uint32_t frac = n % 100;

    /* Digits come out lowest first, so they are collected and copied in reverse.
       do-while, so a value under 1.00 still gets its leading 0. */
    int i = 0;
    do {
        digits[i] = (char)('0' + (whole % 10));
        whole /= 10;
        i++;
    } while (whole > 0);

    while (i > 0) {
        i--;
        s[cur] = digits[i];
        cur++;
    }
    s[cur] = '.';
    cur++;
    s[cur] = (char)('0' + (frac / 10));
    cur++;
    s[cur] = (char)('0' + (frac % 10));
    cur++;
    s[cur] = '\0';

    return cur;
}