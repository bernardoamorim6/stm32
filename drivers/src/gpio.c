#include "../include/gpio.h"
#include "../include/stm32c031_regs.h"
#include "../include/bitops.h"
#include "stdint.h"


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

void gpio_set_af(GPIO_Regs *port, uint32_t pin, gpio_af_t mode){
    port->AFR[pin / 8] = insert(port->AFR[pin / 8], mode, (pin % 8) * 4, 4);
}

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