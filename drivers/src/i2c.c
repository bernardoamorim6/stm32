#include "../include/i2c.h"
#include "../include/stm32c031_regs.h"
#include "bitops.h"
#include "systick.h"
#include <stdint.h>

#define TIMEOUT_MS 30 /* About 23ms per message*/


void i2c_init(I2C_Regs *i2c){
    /* Clear CR1*/
    i2c->CR1 = 0;

    /* Set TIMINGR, values from RM0490's table 114 at page 710.
        TIMINGR = PRESC |   RESERVED  |   SCLDEL    |   SDADEL    |   SCLH  | SCLL
                   0x3         0      |    0x4      |    0x2      |   0x0F  |  0x13
    */
    i2c->TIMINGR = 0x30420F13;

    /* Configure then enable. */
    i2c->CR1 = set_bits(i2c->CR1, make_mask(I2C_CR1_PE, 1));
}

void i2c_xfer_begin(i2c_xfer_t *xfer, uint8_t addr, uint8_t reg, uint8_t *buf,
                    uint8_t len, uint32_t now_ms, uint32_t timeout_ms){
    xfer->state      = I2C_START_WRITE;
    xfer->end_state  = I2C_DONE;
    xfer->addr       = addr;
    xfer->reg        = reg;
    xfer->buf        = buf;
    xfer->len        = len;
    xfer->count      = 0;
    xfer->start_ms   = now_ms;
    xfer->timeout_ms = timeout_ms;
}

i2c_state_t i2c_step(I2C_Regs *i2c, i2c_xfer_t *xfer, uint32_t now_ms){
    if (xfer->state >= I2C_DONE) {
        return xfer->state;
    }
    uint32_t status = i2c->ISR;
    uint32_t berr = extract(status, I2C_ISR_BERR, 1);
    uint32_t arlo = extract(status, I2C_ISR_ARLO, 1);

    if (xfer->state != I2C_START_WRITE && (berr == 1 || arlo == 1)) {
        i2c->ICR = make_mask(I2C_ICR_BERRCF, 1) | make_mask(I2C_ICR_ARLOCF, 1);
        xfer->state = I2C_ERR_BUS;
        return xfer->state;
    }

        
    uint32_t nackf = extract(status, I2C_ISR_NACKF, 1);
    if (xfer->state != I2C_START_WRITE 
        && xfer->state != I2C_WAIT_STOPF 
        && nackf == 1) {
        if (xfer->state == I2C_WAIT_TXIS || xfer->state == I2C_WAIT_RXNE) {
            xfer->end_state = I2C_ERR_NACK_ADDR;
        } else if (xfer->state == I2C_WAIT_TC) {
            xfer->end_state = I2C_ERR_NACK_DATA;
        }
        xfer->state = I2C_WAIT_STOPF;
        return xfer->state;
    }

    switch (xfer->state) {
        case I2C_START_WRITE:
            i2c->CR2 = ((uint32_t)xfer->addr << 1)
                    | ((uint32_t)1 << I2C_CR2_NBYTES)
                    | make_mask(I2C_CR2_START, 1);
            xfer->state = I2C_WAIT_TXIS;
            return xfer->state;
        case I2C_WAIT_TXIS:
            if (extract(status, I2C_ISR_TXIS, 1) == 1) {
                i2c->TXDR = xfer->reg;
                xfer->state = I2C_WAIT_TC;
                return xfer->state;
            }
            break;
        case I2C_WAIT_TC:
            if (extract(status, I2C_ISR_TC, 1) == 1) {
                i2c->CR2 = ((uint32_t)xfer->addr << 1)
                        | ((uint32_t)xfer->len << I2C_CR2_NBYTES)
                        | make_mask(I2C_CR2_AUTOEND, 1)
                        | make_mask(I2C_CR2_START, 1)
                        | make_mask(I2C_CR2_RD_WRN, 1);
                xfer->state = I2C_WAIT_RXNE;
                return xfer->state;
            }
            break;
        case I2C_WAIT_RXNE:
            if (extract(status, I2C_ISR_RXNE, 1) == 1) {
                xfer->buf[xfer->count++] = (uint8_t)i2c->RXDR;
                if (xfer->count == xfer->len) {
                    xfer->state = I2C_WAIT_STOPF;
                }
                return xfer->state;
            }
            break;
        case I2C_WAIT_STOPF:
            if (extract(status, I2C_ISR_STOPF, 1) == 1) {
                i2c->ICR = make_mask(I2C_ICR_STOPCF, 1) 
                          | make_mask(I2C_ICR_NACKCF, 1);
                xfer->state = xfer->end_state;
                return xfer->state;
            }
            break;
        default: break;
    }

    if (now_ms - xfer->start_ms > xfer->timeout_ms) {
        xfer->state = I2C_ERR_TIMEOUT;
    }

    return xfer->state;
}


i2c_state_t i2c_read_reg(I2C_Regs *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len){
    i2c_xfer_t i2c_local;
    i2c_xfer_begin(&i2c_local, addr, reg, buf, len, systick_now(), TIMEOUT_MS);
    while (i2c_local.state < I2C_DONE) {
        i2c_step(i2c, &i2c_local, systick_now());

    }
    return i2c_local.state;
}

