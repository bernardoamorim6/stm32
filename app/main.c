#include "../drivers/include/bitops.h"
#include "../drivers/include/systick.h"
#include "../drivers/include/gpio.h"
#include "../drivers/include/i2c.h"
#include "../drivers/include/usart.h"
#include "../drivers/include/bmp280.h"
#include "../drivers/include/stm32c031_regs.h"
#include "../drivers/include/ssd1306.h"
#include "../drivers/include/formatter.h"


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
        case I2C_WAIT_TXIS_CTRL:
            uart_write_string(USART2, "WAIT_TXIS_CTRL");
            break;
        case I2C_WAIT_TXIS_DATA:
            uart_write_string(USART2, "WAIT_TXIS_DATA");
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
        case I2C_ERR_LEN:
            uart_write_string(USART2, "ERR_LEN");
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

/* The only place that knows where the readings go: the draw calls and the sends
   both use these, so moving a line is one edit. */
#define TEMP_PAGE      0
#define PRESSURE_PAGE  1

/* Sends one page and reports a failure by name */
static void send_page(const uint8_t *screen, uint8_t page) {
    i2c_state_t send_state = ssd1306_send_buffer(I2C1, screen, page, page);
    if (send_state != I2C_DONE) {
        uart_write_string(USART2, "Error sending page to screen ");
        print_state(send_state);
        uart_write_string(USART2, "\r\n");
    }
}

/* Screen goes blank if a reading fails: an old value left up would look current.
   The blank frame is still sent, so the panel never shows stale pixels. Only the
   two reading pages go out, about 23 ms instead of 90 ms for the whole screen. */
static void update_readings(uint8_t *screen, const bmp280_calib_t *cal, i2c_state_t cal_state){
    ssd1306_clear(screen);
    if (cal_state == I2C_DONE){
        uint32_t pascal = 0;
        int32_t temp = 0;
        i2c_state_t meas_state = bmp280_measurement(I2C1, cal, &temp, &pascal);
        if (meas_state != I2C_DONE) {
            uart_write_string(USART2, "Error taking measurements ");
            print_state(meas_state);
            uart_write_string(USART2, "\r\n");
        } else {
            uint8_t t_len;
            uint8_t p_len;
            char temp_string[FORMATTED_MEASUREMENT_SIZE] = {};
            t_len = measurement_formatter(temp, temp_string);
            char pascal_string[FORMATTED_MEASUREMENT_SIZE] = {};
            p_len = measurement_formatter((int32_t)pascal, pascal_string);

            uart_write_string(USART2, "Temperature ");
            uart_write_string(USART2, temp_string);
            uart_write_string(USART2, " C\r\n");
            uart_write_string(USART2, "Pressure ");
            uart_write_string(USART2, pascal_string);   /* Pa are hundredths of a hPa */
            uart_write_string(USART2, " hPa\r\n");

            /* Each unit starts where its number ends, so a longer value such as
               -10.25 never runs into it. The unit's leading space is the gap. */
            ssd1306_draw_string(screen, temp_string, 0, TEMP_PAGE);
            ssd1306_draw_string(screen, " C", (t_len * 6), TEMP_PAGE);
            ssd1306_draw_string(screen, pascal_string, 0, PRESSURE_PAGE);
            ssd1306_draw_string(screen, " hPa", (p_len * 6), PRESSURE_PAGE);
        }
    }
    /* One send per line, so the two can sit on any pages, next to each other or not */
    send_page(screen, TEMP_PAGE);
    send_page(screen, PRESSURE_PAGE);
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
    i2c_state_t oled_state = ssd1306_startup(I2C1);
    uart_write_string(USART2, "SSD1306 startup");
    print_state(oled_state);
    uart_write_string(USART2, "\r\n");
    /* static puts the 1 KB in .bss, not on the stack, and it outlives every frame */
    static uint8_t screen[SSD1306_BUF_SIZE];

    /* Calibration once at boot. Without it any reading is nonsense, so a failure
       is reported and the screen stays blank, but the board keeps running. */
    bmp280_calib_t cal = {0};
    i2c_state_t cal_state = bmp280_read_calib(I2C1, &cal);
    if (cal_state != I2C_DONE) {
        uart_write_string(USART2, "Error taking calibration values ");
        print_state(cal_state);
        uart_write_string(USART2, "\r\n");
    }

    uint32_t now;
    uint32_t last = 0;
    /* The panel's RAM is random at power-on and only the reading pages are sent
       from now on, so the whole (still zeroed) buffer goes out once here */
    oled_state = ssd1306_send_buffer(I2C1, screen, 0, 7);
    if (oled_state != I2C_DONE) {
        uart_write_string(USART2, "Error clearing the screen ");
        print_state(oled_state);
        uart_write_string(USART2, "\r\n");
    }

    uart_write_string(USART2, "echo ready\r\n");
    while (1) {
        echo_one_byte();
        button_drives_led();
        now = systick_now();
        if (now - last >= 1000) {
            update_readings(screen, &cal, cal_state);
            last = now;
        }

    }
    return 0;
}
