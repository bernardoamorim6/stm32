#include "ssd1306.h"
#include "i2c.h"
#include "stm32c031_regs.h"

/* 5x8 glyphs plus a blank column for spacing. One byte per column, bit 0 at
   the top, which is the layout GDDRAM uses, so drawing is a plain copy. */
static const uint8_t font[18][6] = {
    { 0x3E,0x41, 0x41, 0x41, 0x3E, 0x00 },  /* 0 */
    { 0x04, 0x42, 0x7F, 0x40, 0x00, 0x00 }, /* 1 */
    { 0x42, 0x61, 0x51, 0x49, 0x46, 0x00 }, /* 2 */
    { 0x22, 0x41, 0x49, 0x4D, 0x36, 0x00 }, /* 3 */
    { 0x0C, 0x0A, 0x09, 0x7F, 0x08, 0x00 }, /* 4 */
    { 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 }, /* 5 */
    { 0x3C, 0x4A, 0x49, 0x48, 0x30, 0x00 }, /* 6 */
    { 0x03, 0x01, 0x71, 0x09, 0x07, 0x00 }, /* 7 */
    { 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 }, /* 8 */
    { 0x06, 0x09, 0x49, 0x29, 0x1E, 0x00 }, /* 9 */
    { 0x7F, 0x10, 0x08, 0x08, 0x70, 0x00 }, /* h */
    { 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00 }, /* P */
    { 0x30, 0x4A, 0x4A, 0x4A, 0x7C, 0x00 }, /* a */
    { 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 }, /* C */
    { 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 }, /* - */
    { 0x00, 0x60, 0x60, 0x00, 0x00, 0x00 }, /* . */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* space */
    { 0x02, 0x01, 0x51, 0x09, 0x06, 0x00 }  /* ? */
};

/* The whole sequence goes out as one command transfer. RAM is random at power-on,
   so the caller sends a cleared buffer before showing anything. */
i2c_state_t ssd1306_startup(I2C_Regs *i2c){
    static const uint8_t cmds[] =
        {
            0xAE,               /* Display off */
            0xA8, 0x3F,         /* Set MUX Ratio */
            0xD3, 0x00,         /* Set display offset */
            0x40,               /* Set display start line */
            0xA1,               /* Set segment re-map */
            0xC8,               /* Set COM output scan direction */
            0xDA, 0x12,         /* Set COM Pins hardware configuration */
            0x81, 0x7F,         /* Set Contrast Control */
            0xA4,               /* output follows RAM */
            0xA6,               /* Set normal display */
            0xD5, 0x80,         /* Set Osc Frequency */
            0x8D, 0x14,         /* Charge pump on, or the panel stays dark */
            0x20, 0x00,         /* Set mode to horizontal addressing */
            0xAF,               /* Display On */
        };

    return i2c_write_block(i2c, SSD1306_ADDRESS, SSD1306_CTRL_CMD,
                        cmds, sizeof cmds);
}

void ssd1306_clear(uint8_t *buf){
    for (int i = 0; i < SSD1306_BUF_SIZE; i++) {
        buf[i] = 0;
    }
}

/* The range is checked before any bus traffic, so a bad one never reads past buf.
   The window is set to exactly the range first, so the data lands on those pages
   and a frame that failed halfway cannot misalign the next one. Stops at the
   first transfer that does not end in DONE. */
i2c_state_t ssd1306_send_buffer(I2C_Regs *i2c, const uint8_t *buf, uint8_t start_page,
                                uint8_t end_page){
    if (start_page > end_page || end_page > 7) {
        return I2C_ERR_LEN;
    }
    i2c_state_t st;
    const uint8_t cmds[] = {0x21, 0x00, 0x7F, 0x22, start_page, end_page};
    st = i2c_write_block(i2c, SSD1306_ADDRESS, SSD1306_CTRL_CMD,
                        cmds, sizeof cmds);

    /* One page per transfer: the control byte plus 128 fits in NBYTES */
    uint8_t page = start_page;
    while (st == I2C_DONE && page <= end_page) {
        st = i2c_write_block(i2c, SSD1306_ADDRESS, SSD1306_CTRL_DATA, (buf + (page*128)), 128);
        page++;
    }
    return st;
}

/* Only what the readings need. Anything else gets '?', so a missing glyph shows
   on the screen instead of passing for a space. */
static const uint8_t *font_char_helper(char c){
    switch (c) {
        case '0':
            return font[0];
        case '1':
            return font[1];
        case '2':
            return font[2];
        case '3':
            return font[3];
        case '4':
            return font[4];
        case '5':
            return font[5];
        case '6':
            return font[6];
        case '7':
            return font[7];
        case '8':
            return font[8];
        case '9':
            return font[9];
        case 'h':
            return font[10];
        case 'P':
            return font[11];
        case 'a':
            return font[12];
        case 'C':
            return font[13];
        case '-':
            return font[14];
        case '.':
            return font[15];
        case ' ':
            return font[16];
        default:
            return font[17];
    }
}

void ssd1306_draw_char(uint8_t *buf, char c, uint8_t col, uint8_t page){
    const uint8_t *glyph = font_char_helper(c);
    if (page > 7) {
        return;
    }

    /* Clipped per column: past 127 the index lands on the next page, and on
       page 7 past the end of the buffer */
    for (int i = 0; i < 6; i++) {
        if (col + i < 128) {
            buf[(page*128) + col + i] = glyph[i];
        }
    }
}

/* draw_char clips anyway, but col is 8 bits: without the edge check a long
   string wraps past 255 and overwrites the start of the line */
void ssd1306_draw_string(uint8_t *buf, const char *s, uint8_t col, uint8_t page){
    int i = 0;
    while(s[i] != '\0' && col < 128){
        ssd1306_draw_char(buf, s[i], col, page);
        col += 6;
        i += 1;
    }
}

