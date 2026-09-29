#ifndef SYSTICK_H   
#define SYSTICK_H 

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void delay_ms(uint32_t delay);
void systick_init(uint32_t ticks);

#ifdef __cplusplus
}
#endif


#endif //SYSTICK_H
