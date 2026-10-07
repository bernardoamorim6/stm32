#include <stdint.h>
#include "../include/systick.h"

struct stk{
    volatile uint32_t CSR, RVR, CVR, CALIB;
};

#define STK ((struct stk*) 0xE000E010)

void systick_init(uint32_t ticks){
    /* ticks - 1 because the counter runs down through zero, so a period is
       RVR + 1 cycles. Order matters: reload, clear the current value, then
       enable, or the first period runs for whatever CVR happened to hold. */
    STK->RVR = ticks - 1;
    STK->CVR = 0;
    STK->CSR = 0b111;   /* ENABLE | TICKINT | CLKSOURCE = processor clock */
}

/* Written by the handler, read by delay_ms, so it must not be cached in a
   register across the wait loop. */
static volatile uint32_t counter = 0;

/* No registration call: this name overrides the weak alias in startup.c at
   link time. */
void SysTick_Handler(void){
    counter++;
}

void delay_ms(uint32_t delay){
    /* Unsigned subtraction, so the comparison stays correct across the 32-bit
       wrap of counter rather than hanging once every 49 days. */
    uint32_t start = counter;
    while (counter - start < delay) { }
}