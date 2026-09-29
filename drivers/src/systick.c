#include <stdint.h>
#include "../include/systick.h"

struct stk{
    volatile uint32_t CSR, RVR, CVR, CALIB;
};

#define STK ((struct stk*) 0xE000E010)

void systick_init(uint32_t ticks){
    STK->RVR = ticks - 1;
    STK->CVR = 0;
    STK->CSR = 0b111;
}

static volatile uint32_t counter = 0;

void SysTick_Handler(void){
    counter++;
}

void delay_ms(uint32_t delay){
    uint32_t start = counter;
    while (counter - start < delay) { }
}