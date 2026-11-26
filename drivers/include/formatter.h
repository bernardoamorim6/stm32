#ifndef FORMATTER_H
#define FORMATTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "-21474836.48" for INT32_MIN, plus the '\0': the longest text the formatter writes */
#define FORMATTED_MEASUREMENT_SIZE 13

/* Writes a value in hundredths as text with two decimals, so 2143 becomes "21.43"
   and -5 becomes "-0.05". s must hold FORMATTED_MEASUREMENT_SIZE chars. No I/O,
   so the UART and the screen can share it. Returns the length of the measurement
   without '\0'. */
uint8_t measurement_formatter(int32_t value, char *s);

#ifdef __cplusplus
}
#endif

#endif //FORMATTER_H
