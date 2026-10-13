#ifndef I2C_H
#define I2C_H

#include "stm32c031_regs.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit positions from RM0490, only the ones this driver touches. */
#define I2C_CR1_PE       0U    /* I2C enable, the master switch */

#define I2C_CR2_SADD     0U    /* 7-bit address goes in SADD[7:1], so shifted left by one */
#define I2C_CR2_RD_WRN  10U    /* 0 write, 1 read */
#define I2C_CR2_START   13U
#define I2C_CR2_NBYTES  16U    /* 8 bits wide */
#define I2C_CR2_AUTOEND 25U    /* 1: STOP after NBYTES. 0: raise TC instead, for a repeated START */

#define I2C_ISR_TXIS     1U    /* TXDR empty, write the next byte */
#define I2C_ISR_RXNE     2U    /* RXDR full, read it */
#define I2C_ISR_NACKF    4U
#define I2C_ISR_STOPF    5U
#define I2C_ISR_TC       6U    /* NBYTES done with AUTOEND = 0; cleared by setting START */
#define I2C_ISR_BERR     8U
#define I2C_ISR_ARLO     9U

#define I2C_ICR_NACKCF   4U
#define I2C_ICR_STOPCF   5U
#define I2C_ICR_BERRCF   8U
#define I2C_ICR_ARLOCF   9U

/* One state per thing the transfer can be waiting for, plus the outcomes.
   Everything from I2C_DONE onwards is terminal. */
typedef enum {
    I2C_START_WRITE,
    I2C_WAIT_TXIS,
    I2C_WAIT_TC,
    I2C_WAIT_RXNE,
    I2C_WAIT_STOPF,
    I2C_DONE,
    I2C_ERR_NACK_ADDR,
    I2C_ERR_NACK_DATA,
    I2C_ERR_BUS,
    I2C_ERR_TIMEOUT,
} i2c_state_t;

/* Everything one register read needs between calls to i2c_step. */
typedef struct {
    i2c_state_t state;
    i2c_state_t end_state;   /* where WAIT_STOPF finishes: I2C_DONE, or the NACK that sent us there */
    uint8_t  addr;           /* 7-bit, unshifted */
    uint8_t  reg;
    uint8_t *buf;
    uint8_t  len;            /* 1 to 255: NBYTES is 8 bits and RELOAD is not used */
    uint8_t  count;
    uint32_t start_ms;
    uint32_t timeout_ms;
} i2c_xfer_t;

void i2c_init(I2C_Regs *i2c);

/* Prepares a read of len bytes starting at register reg. Touches no hardware. */
void i2c_xfer_begin(i2c_xfer_t *xfer, uint8_t addr, uint8_t reg, uint8_t *buf,
                    uint8_t len, uint32_t now_ms, uint32_t timeout_ms);

/* Advances the transfer by at most one action and returns the new state.
   Never waits: time comes in as now_ms, so the timeout is testable on a host.
   Checks in order: bus errors, NACK, the flag the state waits for, then the
   timeout, which fires once now_ms - start_ms exceeds timeout_ms. */
i2c_state_t i2c_step(I2C_Regs *i2c, i2c_xfer_t *xfer, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif // I2C_H
