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
#define USART2_BASE_ADDR 0x40004400U


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


typedef struct {
    uint32_t volatile CR1;          /*!< USART control register 1,              Address offset: 0x00        */
    uint32_t volatile CR2;          /*!< USART control register 2,              Address offset: 0x04        */
    uint32_t volatile CR3;          /*!< USART control register 3,              Address offset: 0x08        */
    uint32_t volatile BRR;          /*!< USART baud rate register,              Address offset: 0x0C        */
    uint32_t volatile GTPR;         /*!< USART guard time and prescaler reg,    Address offset: 0x10        */
    uint32_t volatile RTOR;         /*!< USART receiver timeout register,       Address offset: 0x14        */
    uint32_t volatile RQR;          /*!< USART request register,                Address offset: 0x18        */
    uint32_t volatile ISR;          /*!< USART interrupt and status register,   Address offset: 0x1C        */
    uint32_t volatile ICR;          /*!< USART interrupt flag clear register,   Address offset: 0x20        */
    uint32_t volatile RDR;          /*!< USART receive data register,           Address offset: 0x24        */
    uint32_t volatile TDR;          /*!< USART transmit data register,          Address offset: 0x28        */
    uint32_t volatile PRESC;        /*!< USART prescaler register,              Address offset: 0x2C        */
} USART_Regs;

/* Same reasoning as GPIO_Regs above. Offsets are from RM0490's USART register
   map, which runs 0x00 to 0x2C with no gaps, so this struct needs no padding.
   CR1 and ISR each have two documented layouts depending on FIFOEN. FIFO mode
   is off at reset and this driver leaves it off, so the non-FIFO layout
   applies. The offsets are the same either way. */
static_assert(offsetof(USART_Regs, CR1)   == 0x00, "USART_Regs: CR1 offset");
static_assert(offsetof(USART_Regs, CR2)   == 0x04, "USART_Regs: CR2 offset");
static_assert(offsetof(USART_Regs, CR3)   == 0x08, "USART_Regs: CR3 offset");
static_assert(offsetof(USART_Regs, BRR)   == 0x0C, "USART_Regs: BRR offset");
static_assert(offsetof(USART_Regs, GTPR)  == 0x10, "USART_Regs: GTPR offset");
static_assert(offsetof(USART_Regs, RTOR)  == 0x14, "USART_Regs: RTOR offset");
static_assert(offsetof(USART_Regs, RQR)   == 0x18, "USART_Regs: RQR offset");
static_assert(offsetof(USART_Regs, ISR)   == 0x1C, "USART_Regs: ISR offset");
static_assert(offsetof(USART_Regs, ICR)   == 0x20, "USART_Regs: ICR offset");
static_assert(offsetof(USART_Regs, RDR)   == 0x24, "USART_Regs: RDR offset");
static_assert(offsetof(USART_Regs, TDR)   == 0x28, "USART_Regs: TDR offset");
static_assert(offsetof(USART_Regs, PRESC) == 0x2C, "USART_Regs: PRESC offset");
static_assert(sizeof(USART_Regs)          == 0x30, "USART_Regs: total size");

/* Define the USART2 struct */
#define USART2 ((USART_Regs *)(USART2_BASE_ADDR))


#ifdef __cplusplus
}
#endif


#endif //STM32C031_REGS_H

