#ifndef STM32C031_REGS_H   
#define STM32C031_REGS_H 

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPIOA_BASE_ADDR 0x50000000U
#define GPIOC_BASE_ADDR 0x50000800U


typedef struct {
    uint32_t volatile MODER;        /*!< GPIO port mode register,               Address offset: 0x00        */ 
    uint32_t volatile OTYPER;       /*!< GPIO port output type register,        Address offset: 0x04        */ 
    uint32_t volatile OSPEEDR;      /*!< GPIO port output speed register,       Address offset: 0x08        */ 
    uint32_t volatile PUPDR;        /*!< GPIO port pull-up/pull-down register,  Address offset: 0x0C        */  
    uint32_t volatile IDR;          /*!< GPIO port input data register,         Address offset: 0x10        */ 
    uint32_t volatile ODR;          /*!< GPIO port output data register,        Address offset: 0x14        */ 
    uint32_t volatile BSRR;         /*!< GPIO port bit set/reset  register,     Address offset: 0x18        */ 
    uint32_t volatile LCKR;         /*!< GPIO port configuration lock register, Address offset: 0x1C        */ 
    uint32_t volatile AFR[2];       /*!< GPIO alternate function registers,     Address offset: 0x20-0x24   */ 
    uint32_t volatile BRR;          /*!< GPIO Bit Reset register,               Address offset: 0x28        */ 
} GPIO_Regs;

/* The struct layout is the hardware memory map, so a member added, reordered
   or mis-padded silently redirects every access after it. These checks are
   compile-time only and make this a build failure instead. Offsets are from
   RM0490's GPIO register map. */
static_assert(offsetof(GPIO_Regs, MODER)   == 0x00, "GPIO_Regs: MODER offset");
static_assert(offsetof(GPIO_Regs, OTYPER)  == 0x04, "GPIO_Regs: OTYPER offset");
static_assert(offsetof(GPIO_Regs, OSPEEDR) == 0x08, "GPIO_Regs: OSPEEDR offset");
static_assert(offsetof(GPIO_Regs, PUPDR)   == 0x0C, "GPIO_Regs: PUPDR offset");
static_assert(offsetof(GPIO_Regs, IDR)     == 0x10, "GPIO_Regs: IDR offset");
static_assert(offsetof(GPIO_Regs, ODR)     == 0x14, "GPIO_Regs: ODR offset");
static_assert(offsetof(GPIO_Regs, BSRR)    == 0x18, "GPIO_Regs: BSRR offset");
static_assert(offsetof(GPIO_Regs, LCKR)    == 0x1C, "GPIO_Regs: LCKR offset");
static_assert(offsetof(GPIO_Regs, AFR)     == 0x20, "GPIO_Regs: AFR offset");
static_assert(offsetof(GPIO_Regs, BRR)     == 0x28, "GPIO_Regs: BRR offset");
static_assert(sizeof(GPIO_Regs)            == 0x2C, "GPIO_Regs: total size");

/* Define GPIOA and GPIOC Structs */
#define GPIOA ((GPIO_Regs *)(GPIOA_BASE_ADDR))
#define GPIOC ((GPIO_Regs *)(GPIOC_BASE_ADDR))


#ifdef __cplusplus
}
#endif


#endif //STM32C031_REGS_H

