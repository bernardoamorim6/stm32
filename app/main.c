#include "../drivers/include/bitops.h"
#include "../drivers/include/systick.h"
#include <stdint.h>

int globalVariable1 = 0xDEADBEEF;
int globalVariable2;

int main(void) {
    uint32_t volatile *RCC_IOPENR = (uint32_t volatile *)(0x40021000+0x34);
    *RCC_IOPENR |= 1;
    uint32_t volatile *GPIOA_MODER = (uint32_t volatile *)(0x50000000 + 0x00);
    *GPIOA_MODER = insert(*GPIOA_MODER, 1, 10, 2);
    uint32_t volatile *RCC_CR = (uint32_t volatile *)(0x40021000);
    /* Clock in regular value of dividing by 4 aka 12MHz frequency */
    *RCC_CR = insert(*RCC_CR, 0b010, 11, 3);
    globalVariable1++;
    globalVariable2 = 5;
    uint32_t volatile *GPIOA_ODR = (uint32_t volatile *)(0x50000000 + 0x14);
    systick_init(12000);
    while (1) {
        *GPIOA_ODR = insert(*GPIOA_ODR, 1, 5, 1);
        delay_ms(500);
        *GPIOA_ODR = insert(*GPIOA_ODR, 0, 5, 1);
        delay_ms(500);
    }
    return 0;
}
