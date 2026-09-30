#ifndef GPIO_H   
#define GPIO_H 

#include "stm32c031_regs.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { 
    GPIO_MODE_INPUT = 0,        /* Input */
    GPIO_MODE_OUTPUT = 1,       /* Output */
    GPIO_MODE_ALTERNATE = 2,    /* Alternate Function */
    GPIO_MODE_ANALOG = 3,       /* Analog */
} gpio_mode_t;

typedef enum { 
    GPIO_TYPE_PUSH_PULL= 0,     /* Output pus-puçç (reset state) */
    GPIO_TYPE_OPEN_DRAIN = 1,   /* Output Open Drain */
} gpio_type_t;

typedef enum { 
    GPIO_SPEED_VLOW= 0,         /* Very low speed */
    GPIO_SPEED_LOW = 1,         /* Low speed */
    GPIO_SPEED_HIGH = 2,        /* High speed */
    GPIO_SPEED_VHIGH = 3,       /* Very high speed */
} gpio_speed_t;

typedef enum { 
    GPIO_PUPD_NUND= 0,          /* No pull-up, no pull-down */
    GPIO_PUPD_PU = 1,           /* Pull-up */
    GPIO_PUPD_PD = 2,           /* Pull-down */
    /* Last value is reserved*/
} gpio_pupd_t;


typedef enum { 
    GPIO_AFR_AF0 = 0x0,         
    GPIO_AFR_AF1 = 0x1,
    GPIO_AFR_AF2= 0x2,
    GPIO_AFR_AF3= 0x3,
    GPIO_AFR_AF4= 0x4,
    GPIO_AFR_AF5= 0x5,
    GPIO_AFR_AF6= 0x6,
    GPIO_AFR_AF7= 0x7,
    GPIO_AFR_AF8= 0x8,
    GPIO_AFR_AF9= 0x9,
    GPIO_AFR_AF10= 0xA,
    GPIO_AFR_AF11= 0xB,
    GPIO_AFR_AF12= 0xC,
    GPIO_AFR_AF13= 0xD,
    GPIO_AFR_AF14= 0xE,
    GPIO_AFR_AF15= 0xF,
} gpio_af_t;


void gpio_set_mode(GPIO_Regs *port, uint32_t pin, gpio_mode_t mode);
void gpio_set_ospeed(GPIO_Regs *port, uint32_t pin, gpio_speed_t speed);
void gpio_set_otype(GPIO_Regs *port, uint32_t pin, gpio_type_t type);
void gpio_set_pull(GPIO_Regs *port, uint32_t pin, gpio_pupd_t pull);
void gpio_set_af(GPIO_Regs *port, uint32_t pin, gpio_af_t mode);
void gpio_write(GPIO_Regs *port, uint32_t pin, uint32_t value);
uint32_t gpio_read(GPIO_Regs *port, uint32_t pin);


#ifdef __cplusplus
}
#endif


#endif //GPIO_H
