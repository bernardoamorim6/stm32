
#ifndef SSD1306_H
#define SSD1306_H

#include "i2c.h"
#include "stm32c031_regs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_ADDRESS     0x3CU    /* SA0 low: 0x78 on the back of the board */
#define SSD1306_CTRL_CMD    0x00U    /* SSD1306 control byte that makes all next bytes commands */
#define SSD1306_CTRL_DATA   0x40U    /* SSD1306 control byte that makes all next bytes data bytes */
#define SSD1306_BUF_SIZE 1024

i2c_state_t ssd1306_startup(I2C_Regs *i2c);
void ssd1306_clear(uint8_t *buf);
i2c_state_t ssd1306_send_buffer(I2C_Regs *i2c, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H
