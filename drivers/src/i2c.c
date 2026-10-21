#include "../include/i2c.h"
#include "../include/stm32c031_regs.h"
#include "bitops.h"
#include "systick.h"
#include <stddef.h>
#include <stdint.h>

/* The longest read, 258 bytes of 9 clocks each at 100 kHz, takes about 23 ms */
#define TIMEOUT_MS 30


void i2c_init(I2C_Regs *i2c){
    /* TIMINGR can only be written while PE = 0 */
    i2c->CR1 = 0;

    /* RM0490 table 114, 100 kHz from a 16 MHz kernel clock: PRESC 3, SCLDEL 4,
       SDADEL 2, SCLH 0x0F, SCLL 0x13. Measured on the bus: 103.5 kHz. */
    i2c->TIMINGR = 0x30420F13;

    i2c->CR1 = set_bits(i2c->CR1, make_mask(I2C_CR1_PE, 1));
}

void i2c_xfer_begin_read(i2c_xfer_t *xfer, uint8_t addr, uint8_t reg, uint8_t *buf,
                    uint8_t len, uint32_t now_ms, uint32_t timeout_ms){
    xfer->state      = I2C_START_WRITE;
    xfer->end_state  = I2C_DONE;
    xfer->addr       = addr;
    xfer->reg        = reg;
    xfer->buf        = buf;
    xfer->len        = len;
    xfer->value     = 0;
    xfer->count      = 0;
    xfer->start_ms   = now_ms;
    xfer->timeout_ms = timeout_ms;
}


void i2c_xfer_begin_write(i2c_xfer_t *xfer, uint8_t addr, uint8_t reg, uint8_t value,
                    uint32_t now_ms, uint32_t timeout_ms){
    xfer->state      = I2C_START_WRITE;
    xfer->end_state  = I2C_DONE;
    xfer->addr       = addr;
    xfer->reg        = reg;
    xfer->buf        = NULL;
    xfer->len        = 0;
    xfer->value      = value;
    xfer->count      = 0;
    xfer->start_ms   = now_ms;
    xfer->timeout_ms = timeout_ms;
}


i2c_state_t i2c_step_read(I2C_Regs *i2c, i2c_xfer_t *xfer, uint32_t now_ms){
    if (xfer->state >= I2C_DONE) {
        return xfer->state;
    }
    /* One read of ISR, so every check below sees the same snapshot */
    uint32_t status = i2c->ISR;
    uint32_t berr = extract(status, I2C_ISR_BERR, 1);
    uint32_t arlo = extract(status, I2C_ISR_ARLO, 1);

    /* START_WRITE acts without looking at ISR, so errors are left to the next step */
    if (xfer->state != I2C_START_WRITE && (berr == 1 || arlo == 1)) {
        i2c->ICR = make_mask(I2C_ICR_BERRCF, 1) | make_mask(I2C_ICR_ARLOCF, 1);
        xfer->state = I2C_ERR_BUS;
        return xfer->state;
    }

    /* After a NACK the peripheral sends STOP by itself, so wait for it and keep
       the error in end_state. NACKF stays set until cleared, hence not in WAIT_STOPF. */
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
            /* One store: START fires immediately, so the address and count must be in
               the same write. AUTOEND = 0 makes the peripheral raise TC instead of STOP. */
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
            /* START here is the repeated START, and setting it is also what clears TC.
               The peripheral NACKs byte NBYTES, and AUTOEND = 1 makes it send STOP. */
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
            /* Reading RXDR is what clears RXNE */
            if (extract(status, I2C_ISR_RXNE, 1) == 1) {
                xfer->buf[xfer->count++] = (uint8_t)i2c->RXDR;
                if (xfer->count == xfer->len) {
                    xfer->state = I2C_WAIT_STOPF;
                }
                return xfer->state;
            }
            break;
        case I2C_WAIT_STOPF:
            /* NACKCF as well: needed after a NACK, and a no-op after a clean read */
            if (extract(status, I2C_ISR_STOPF, 1) == 1) {
                i2c->ICR = make_mask(I2C_ICR_STOPCF, 1)
                          | make_mask(I2C_ICR_NACKCF, 1);
                xfer->state = xfer->end_state;
                return xfer->state;
            }
            break;
        default: break;
    }

    /* Unsigned subtraction, so this stays correct across the 32-bit tick wrap */
    if (now_ms - xfer->start_ms > xfer->timeout_ms) {
        xfer->state = I2C_ERR_TIMEOUT;
    }

    return xfer->state;
}

/* Blocking, and the only way out of a dead bus is the timeout. So never call it
   from an interrupt handler: SysTick cannot preempt it, and time would stop. */
i2c_state_t i2c_read_reg(I2C_Regs *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len){
    i2c_xfer_t i2c_local;
    i2c_xfer_begin_read(&i2c_local, addr, reg, buf, len, systick_now(), TIMEOUT_MS);
    while (i2c_local.state < I2C_DONE) {
        i2c_step_read(i2c, &i2c_local, systick_now());
    }
    return i2c_local.state;
}


i2c_state_t i2c_step_write(I2C_Regs *i2c, i2c_xfer_t *xfer, uint32_t now_ms){
    if (xfer->state >= I2C_DONE) {
        return xfer->state;
    }
    /* One read of ISR, so every check below sees the same snapshot */
    uint32_t status = i2c->ISR;
    uint32_t berr = extract(status, I2C_ISR_BERR, 1);
    uint32_t arlo = extract(status, I2C_ISR_ARLO, 1);

    /* START_WRITE acts without looking at ISR, so errors are left to the next step */
    if (xfer->state != I2C_START_WRITE && (berr == 1 || arlo == 1)) {
        i2c->ICR = make_mask(I2C_ICR_BERRCF, 1) | make_mask(I2C_ICR_ARLOCF, 1);
        xfer->state = I2C_ERR_BUS;
        return xfer->state;
    }

    /* A NACK on the address or register byte: the peripheral sends STOP by itself,
       so wait for it. A NACK on the value byte comes after the last TXIS, see WAIT_STOPF. */
    uint32_t nackf = extract(status, I2C_ISR_NACKF, 1);
    if (xfer->state != I2C_START_WRITE
        && xfer->state != I2C_WAIT_STOPF
        && nackf == 1) {
        if (xfer->state == I2C_WAIT_TXIS_REG) {
            xfer->end_state = I2C_ERR_NACK_ADDR;
        } else if (xfer->state == I2C_WAIT_TXIS_VALUE) {
            xfer->end_state = I2C_ERR_NACK_DATA;
        }
        xfer->state = I2C_WAIT_STOPF;
        return xfer->state;
    }

    switch (xfer->state) {
        case I2C_START_WRITE:
            /* One store: START fires immediately, so the address and count must be in the
               same write. NBYTES 2 is register and value; AUTOEND 1 sends STOP after them. */
            i2c->CR2 = ((uint32_t)xfer->addr << 1)
                    | ((uint32_t)2 << I2C_CR2_NBYTES)
                    | make_mask(I2C_CR2_AUTOEND, 1)
                    | make_mask(I2C_CR2_START, 1);
            xfer->state = I2C_WAIT_TXIS_REG;
            return xfer->state;
        case I2C_WAIT_TXIS_REG:
            if (extract(status, I2C_ISR_TXIS, 1) == 1) {
                i2c->TXDR = xfer->reg;
                xfer->state = I2C_WAIT_TXIS_VALUE;
                return xfer->state;
            }
            break;
        case I2C_WAIT_TXIS_VALUE:
            if (extract(status, I2C_ISR_TXIS, 1) == 1) {
                i2c->TXDR = xfer->value;
                xfer->state = I2C_WAIT_STOPF;
                return xfer->state;
            }
            break;
        case I2C_WAIT_STOPF:
            /* NACKF with no earlier error means the value byte was refused */
            if (extract(status, I2C_ISR_STOPF, 1) == 1) {
                i2c->ICR = make_mask(I2C_ICR_STOPCF, 1)
                          | make_mask(I2C_ICR_NACKCF, 1);
                if (nackf == 1 && xfer->end_state == I2C_DONE){
                    xfer->end_state = I2C_ERR_NACK_DATA;
                }
                xfer->state = xfer->end_state;
                return xfer->state;
            }
            break;
        default: break;
    }

    /* Unsigned subtraction, so this stays correct across the 32-bit tick wrap */
    if (now_ms - xfer->start_ms > xfer->timeout_ms) {
        xfer->state = I2C_ERR_TIMEOUT;
    }

    return xfer->state;
}

/* Blocking, and the only way out of a dead bus is the timeout. So never call it
   from an interrupt handler: SysTick cannot preempt it, and time would stop. */
i2c_state_t i2c_write_reg(I2C_Regs *i2c, uint8_t addr, uint8_t reg, uint8_t value){
    i2c_xfer_t i2c_local;
    i2c_xfer_begin_write(&i2c_local, addr, reg, value, systick_now(), TIMEOUT_MS);
    while (i2c_local.state < I2C_DONE) {
        i2c_step_write(i2c, &i2c_local, systick_now());
    }
    return i2c_local.state;
}
