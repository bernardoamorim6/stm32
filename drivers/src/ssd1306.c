#include "ssd1306.h"
#include "i2c.h"
#include "stm32c031_regs.h"

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
            0x8D, 0x14,         /* Enable charge pump regulator */
            0x20, 0x00,         /* Set mode to horizontal addressing */
            0xAF,               /* Display On */
        };

    return i2c_write_block(i2c, SSD1306_ADDRESS, SSD1306_CTRL_CMD, 
                        cmds, sizeof cmds);
}

void ssd1306_clear(uint8_t *buf){
    for (int i = 0; i < 1024; i++) {
        buf[i] = 0;
    }
}

i2c_state_t ssd1306_send_buffer(I2C_Regs *i2c, const uint8_t *buf){
    i2c_state_t st;
    static const uint8_t cmds[] = {0x21, 0x00, 0x7F, 0x22, 0x00, 0x07};
    st = i2c_write_block(i2c, SSD1306_ADDRESS, SSD1306_CTRL_CMD, 
                        cmds, sizeof cmds);

    uint8_t page = 0;
    while (st == I2C_DONE && page < 8) {
        st = i2c_write_block(i2c, SSD1306_ADDRESS, SSD1306_CTRL_DATA, (buf + page*128), 128);
        page++;
    }
    return st;
}

