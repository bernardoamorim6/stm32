#include "../drivers/include/bitops.h"
#include "../drivers/include/systick.h"
#include "gpio.h"
#include "i2c.h"
#include "usart.h"
#include "stm32c031_regs.h"
#include <stdint.h>


static void clock_init(void){
    uint32_t volatile *RCC_CR = (uint32_t volatile *)(0x40021000);
    /* HSIDIV = ÷4, the reset value, written anyway: 48 MHz / 4 = 12 MHz */
    *RCC_CR = insert(*RCC_CR, 0b010, 11, 3);
    /* HSITRIM 62 instead of 64 took the 100 Hz test blink from +0.5% to -0.06%.
       Measured on this chip, another one may need a different value. */
    uint32_t volatile *RCC_ICSCR = (uint32_t volatile *)(0x40021000 + 0x04);
    *RCC_ICSCR = insert(*RCC_ICSCR, 62, 8, 7);

    uint32_t volatile *RCC_IOPENR = (uint32_t volatile *)(0x40021000+0x34);
    *RCC_IOPENR |= 0b111; /* GPIOA | GPIOB | GPIOC */

    uint32_t volatile *RCC_APBENR1 = (uint32_t volatile *)(0x40021000+0x3c);
    *RCC_APBENR1 |= make_mask(17, 1) | make_mask(21, 1); /* USART2EN | I2C1EN */

    /* I2C1SEL = HSIKER (16 MHz), the clock RM0490's TIMINGR table assumes */
    uint32_t volatile *RCC_CCIPR = (uint32_t volatile *)(0x40021000+0x54);
    *RCC_CCIPR = insert(*RCC_CCIPR, 0b10, 12, 2);
}

static void pins_init(void){
    /* PA5: user LED. PC13: user button, low when pressed */
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    gpio_set_otype(GPIOA, 5, GPIO_TYPE_PUSH_PULL);
    gpio_set_ospeed(GPIOA, 5, GPIO_SPEED_VLOW);
    gpio_set_mode(GPIOC, 13, GPIO_MODE_INPUT);
    gpio_set_pull(GPIOC, 13, GPIO_PUPD_PU);

    /* PA2/PA3: USART2 TX/RX, routed to the ST-LINK virtual COM port */
    gpio_set_mode(GPIOA, 2, GPIO_MODE_ALTERNATE);
    gpio_set_mode(GPIOA, 3, GPIO_MODE_ALTERNATE);
    gpio_set_af(GPIOA, 2, GPIO_AFR_AF1);
    gpio_set_af(GPIOA, 3, GPIO_AFR_AF1);

    /* PB8/PB9: I2C1 SCL/SDA on CN5 pins 10 and 9. Open-drain, and the mode is
       set last so the pin is never handed over half configured. */
    gpio_set_otype(GPIOB, 8, GPIO_TYPE_OPEN_DRAIN);
    gpio_set_otype(GPIOB, 9, GPIO_TYPE_OPEN_DRAIN);
    gpio_set_pull(GPIOB, 8, GPIO_PUPD_PU);
    gpio_set_pull(GPIOB, 9, GPIO_PUPD_PU);
    gpio_set_af(GPIOB, 8, GPIO_AFR_AF6);
    gpio_set_af(GPIOB, 9, GPIO_AFR_AF6);
    gpio_set_mode(GPIOB, 8, GPIO_MODE_ALTERNATE);
    gpio_set_mode(GPIOB, 9, GPIO_MODE_ALTERNATE);
}

/* A raw byte would show up as its ASCII character, so send one hex digit per nibble */
static void print_hex(uint8_t value) {
    char low_nibble = "0123456789ABCDEF"[value & 0xF];
    char high_nibble = "0123456789ABCDEF"[value >> 4];
    uart_write_byte(USART2, '0');
    uart_write_byte(USART2, 'x');
    uart_write_byte(USART2, high_nibble);
    uart_write_byte(USART2, low_nibble);
}

/* The i2c_state_t, prints a string with the state name */
static void print_state(i2c_state_t st) {
    uart_write_string(USART2, " state ");
    switch (st) {
        case I2C_START_WRITE:
            uart_write_string(USART2, "START_WRITE");
            break;
        case I2C_WAIT_TXIS:
            uart_write_string(USART2, "WAIT_TXIS");
            break;
        case I2C_WAIT_TC:
            uart_write_string(USART2, "WAIT_TC");
            break;
        case I2C_WAIT_RXNE:
            uart_write_string(USART2, "WAIT_RXNE");
            break;
        case I2C_WAIT_STOPF:
            uart_write_string(USART2, "WAIT_STOPF");
            break;
        case I2C_WAIT_TXIS_REG:
            uart_write_string(USART2, "WAIT_TXIS_REG");
            break;
        case I2C_WAIT_TXIS_VALUE:
            uart_write_string(USART2, "WAIT_TXIS_VALUE");
            break;
        case I2C_DONE:
            uart_write_string(USART2, "DONE");
            break;
        case I2C_ERR_NACK_ADDR:
            uart_write_string(USART2, "ERR_NACK_ADDR");
            break;
        case I2C_ERR_NACK_DATA:
            uart_write_string(USART2, "ERR_NACK_DATA");
            break;
        case I2C_ERR_BUS:
            uart_write_string(USART2, "ERR_BUS");
            break;
        case I2C_ERR_TIMEOUT:
            uart_write_string(USART2, "ERR_TIMEOUT");
            break;
        default:
            break;
    }
}

/* Every valid 7-bit address. A NACK means nobody is there; anything other
   than DONE or a NACK is a bus problem, so it is printed too. */
static void i2c_bus_scan(void) {
    uart_write_string(USART2, "scanning\r\n");
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        uint8_t b;
        i2c_state_t st = i2c_read_reg(I2C1, addr, 0x00, &b, 1);
        if (st == I2C_DONE) {
            print_hex(addr);
            uart_write_string(USART2, "\r\n");
        } else if (st != I2C_ERR_NACK_ADDR) {
            print_hex(addr);
            print_state(st);
            uart_write_string(USART2, "\r\n");
        }
    }
    uart_write_string(USART2, "scan done\r\n");
}

/* An answer at an address only proves something is there; the ID says what */
static void print_chip_id(const char *name, uint8_t addr, uint8_t reg) {
    uint8_t id = 0;
    i2c_state_t st = i2c_read_reg(I2C1, addr, reg, &id, 1);
    uart_write_string(USART2, name);
    uart_write_string(USART2, " id ");
    print_hex(id);
    print_state(st);
    uart_write_string(USART2, "\r\n");
}

/* 0x88 to 0x9F in one read: the BMP280 advances its register pointer by itself.
   Zeroed, so a short read shows up as zeros rather than old stack contents. */
static void print_calibration(void){
    uint8_t calibration[24] = {0};
    i2c_state_t st = i2c_read_reg(I2C1, 0x76, 0x88, calibration, sizeof(calibration));
    uart_write_string(USART2, "calibration: ");
    for (unsigned long c = 0; c < sizeof(calibration); c++) {
        if (c > 0) {
            uart_write_string(USART2, " ");
        }
        print_hex(calibration[c]);
    }
    print_state(st);
    uart_write_string(USART2, "\r\n");
}

static void print_measurement(void){
    i2c_state_t st = i2c_write_reg(I2C1, 0x76, 0xF4, 0x25);
    uart_write_string(USART2, "write ctrl_meas");
    print_state(st);
    uart_write_string(USART2, "\r\n");
    delay_ms(10);
    uint8_t ctrl_meas = 0;
    i2c_read_reg(I2C1, 0x76, 0xF4, &ctrl_meas, 1);
    uart_write_string(USART2, "ctrl_meas: ");
    print_hex(ctrl_meas);
    uart_write_string(USART2,"\r\n");
    uint8_t data[6] = {0};
    st = i2c_read_reg(I2C1, 0x76, 0xF7, data, sizeof(data));
    uart_write_string(USART2, "data: ");
    for (unsigned long c = 0; c < sizeof(data); c++) {
        if (c > 0) {
            uart_write_string(USART2, " ");
        }
        print_hex(data[c]);
    }
    print_state(st);
    uart_write_string(USART2, "\r\n");
}

/* Neither call blocks on the wire, so the button stays responsive. */
static void echo_one_byte(void) {
    uint8_t byte;
    if (uart_read_byte(&byte)) {
        uart_write_byte(USART2, byte);
    }
}

static void button_drives_led(void) {
    gpio_write(GPIOA, 5, gpio_read(GPIOC, 13) == 0);
}

int main(void) {
    clock_init();
    pins_init();

    uint32_t volatile *NVIC_ISER = (uint32_t volatile *)0xE000E100;
    *NVIC_ISER = make_mask(28, 1); /* USART2 is IRQ 28 */

    systick_init(12000);
    uart_init(USART2, 12000000, 115200);
    i2c_init(I2C1);

    i2c_bus_scan();
    print_chip_id("bmp280", 0x76, 0xD0);    /* expect 0x60 for a BME280, 0x58 for a BMP280 */
    print_chip_id("mpu6050", 0x68, 0x75);   /* expect 0x68 */
    print_calibration();
    print_measurement();

    uart_write_string(USART2, "echo ready\r\n");
    while (1) {
        echo_one_byte();
        button_drives_led();
    }
    return 0;
}
