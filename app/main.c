#include "../drivers/include/bitops.h"
#include "../drivers/include/systick.h"
#include "gpio.h"
#include "stm32c031_regs.h"
#include <stdint.h>


static void clock_init(void){
    uint32_t volatile *RCC_CR = (uint32_t volatile *)(0x40021000);
    /* Clock in regular value of dividing by 4 aka 12MHz frequency */
    *RCC_CR = insert(*RCC_CR, 0b010, 11, 3);
    /* Set HSITRIM to a lower value than 64 in order to improve frequecny from 100.5 to 99.94Hz, 
    This is specific to this chip and could require a different value on another chip */
    uint32_t volatile *RCC_ICSCR = (uint32_t volatile *)(0x40021000 + 0x04);
    *RCC_ICSCR = insert(*RCC_ICSCR, 62, 8, 7);

    uint32_t volatile *RCC_IOPENR = (uint32_t volatile *)(0x40021000+0x34);
    *RCC_IOPENR |= 0b101;
}

int main(void) {
    /* Initialize the clock */
    clock_init();

    // uint32_t volatile *GPIOA_MODER = (uint32_t volatile *)(0x50000000 + 0x00);
    // *GPIOA_MODER = insert(*GPIOA_MODER, 1, 10, 2);
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    gpio_set_otype(GPIOA, 5, GPIO_TYPE_PUSH_PULL);
    gpio_set_ospeed(GPIOA, 5, GPIO_SPEED_VLOW);

    gpio_set_mode(GPIOC, 13, GPIO_MODE_INPUT);
    gpio_set_pull(GPIOC, 13, GPIO_PUPD_PU);



    // uint32_t volatile *GPIOA_ODR = (uint32_t volatile *)(0x50000000 + 0x14);
    systick_init(12000);
    while (1) {
        // *GPIOA_ODR = insert(*GPIOA_ODR, 1, 5, 1);
        // delay_ms(500);
        // *GPIOA_ODR = insert(*GPIOA_ODR, 0, 5, 1);
        // delay_ms(500);

        if (gpio_read(GPIOC, 13) == 0) {
            gpio_write(GPIOA, 5, 1);
        } else {
            gpio_write(GPIOA, 5, 0);
        }
    }
    return 0;
}
