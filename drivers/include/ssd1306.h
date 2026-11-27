#ifndef SSD1306_H
#define SSD1306_H

#include "i2c.h"
#include "stm32c031_regs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_ADDRESS     0x3CU    /* SA0 low: 0x78 on the back of the board */
#define SSD1306_CTRL_CMD    0x00U    /* control byte: every byte after it is a command */
#define SSD1306_CTRL_DATA   0x40U    /* control byte: every byte after it is pixel data */
#define SSD1306_BUF_SIZE    1024     /* 8 pages x 128 columns, one byte per 8-pixel column */

/* Bus side. Each returns the state its I2C transfer ended in. */
i2c_state_t ssd1306_startup(I2C_Regs *i2c);
/* Sends pages start_page..end_page of buf and leaves the rest of the panel as
   it is. A start after the end, or a page past 7, sends nothing: I2C_ERR_LEN. */
i2c_state_t ssd1306_send_buffer(I2C_Regs *i2c, const uint8_t *buf, uint8_t start_page,
                                uint8_t end_page);

/* Buffer side: RAM only, no bus, so all of it is tested on the host. */
void ssd1306_clear(uint8_t *buf);

/* Draws c with its left column at (col, page). Columns past 127 are clipped,
    page > 7 draws nothing, and a character not in the font draws as '?'. */
void ssd1306_draw_char(uint8_t *buf, char c, uint8_t col, uint8_t page);

/* Draws s left to right from (col, page), 6 columns per character. Stops at the
   end of the string or the right edge; the last character that fits is clipped. */
void ssd1306_draw_string(uint8_t *buf, const char *s, uint8_t col, uint8_t page);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H
