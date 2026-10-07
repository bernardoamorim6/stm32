#include "../include/gpio.h"
#include "../include/stm32c031_regs.h"
#include "../include/bitops.h"
#include "stdint.h"


/* Field width per pin is not uniform across these registers and nothing warns
   you if you get it wrong: MODER, OSPEEDR and PUPDR are 2 bits at pin * 2,
   OTYPER and IDR are 1 bit at pin, and AFR is 4 bits spread over two words. */

void gpio_set_mode(GPIO_Regs *port, uint32_t pin, gpio_mode_t mode){
    port->MODER = insert(port->MODER, mode, pin * 2, 2);
}


void gpio_set_ospeed(GPIO_Regs *port, uint32_t pin, gpio_speed_t speed){
    port->OSPEEDR = insert(port->OSPEEDR, speed, pin * 2, 2);
}

void gpio_set_otype(GPIO_Regs *port, uint32_t pin, gpio_type_t type){
     port->OTYPER = insert(port->OTYPER, type, pin , 1);
}

void gpio_set_pull(GPIO_Regs *port, uint32_t pin, gpio_pupd_t pull){
    port->PUPDR = insert(port->PUPDR, pull, pin*2, 2);
}

/* 16 pins at 4 bits each needs 64 bits, so the field lives in AFR[0] for pins
   0-7 and AFR[1] for 8-15. */
void gpio_set_af(GPIO_Regs *port, uint32_t pin, gpio_af_t mode){
    port->AFR[pin / 8] = insert(port->AFR[pin / 8], mode, (pin % 8) * 4, 4);
}

/* BSRR rather than a read-modify-write on ODR: bits 15:0 set, 31:16 clear,
   zeros do nothing. One store, so an interrupt touching the same port cannot
   land in the middle of it. It is write-only, a read returns 0. */
void gpio_write(GPIO_Regs *port, uint32_t pin, uint32_t value){
    if (value == 1) {
        port->BSRR = 1u << pin;
    } else {
        port->BSRR = 1u << (pin + 16);
    }
}

uint32_t gpio_read(GPIO_Regs *port, uint32_t pin){
    return extract(port->IDR, pin , 1);
}