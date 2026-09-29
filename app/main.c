#include "../drivers/include/bitops.h"
#include <stdint.h>

int globalVariable1 = 0xDEADBEEF;
int globalVariable2;

int main(void) {
    uint32_t volatile *RCC_IOPENR = (uint32_t volatile *)(0x40021000+0x34);
    *RCC_IOPENR |= 1;
    uint32_t volatile *GPIOA_MODER = (uint32_t volatile *)(0x50000000 + 0x00);
    *GPIOA_MODER = insert(*GPIOA_MODER, 1, 10, 2);
    uint32_t volatile *RCC_CR = (uint32_t volatile *)(0x40021000);
    *RCC_CR = insert(*RCC_CR, 0b011, 11, 3);
    globalVariable1++;
    globalVariable2 = 5;
    uint32_t volatile *GPIOA_ODR = (uint32_t volatile *)(0x50000000 + 0x14);
    while (1) {
        int volatile counter = 0;
        *GPIOA_ODR = insert(*GPIOA_ODR, 1, 5, 1);

        while(counter < 1000000){
            counter++;
        }
        counter = 0;
        *GPIOA_ODR = insert(*GPIOA_ODR, 0, 5, 1);

        while(counter < 500000){
            counter++;
        }
    }
    return 0;
}
