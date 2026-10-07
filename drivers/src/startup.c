// Startup code for the STM32C031 board

#include <stdint.h>

typedef void (*isr_handler_t)(void);

extern uint32_t _la_data, _sdata, _edata, _sbss, _ebss;
extern int main();

void Reset_Handler(void){
    // Copy .data from Flash to RAM
    const uint32_t *src = &_la_data;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    // Zero-initialize .bss
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }
    // Call main()
    main();
    while(1) {}
}

extern uint32_t top_of_stack;
void DefaultHandler(void) {
    while (1) {}
}
void NMI_Handler(void) __attribute__((weak, alias("DefaultHandler")));
void HardFault_Handler(void) __attribute__((weak, alias("DefaultHandler")));
void SVCall_Handler(void) __attribute__((weak, alias("DefaultHandler")));
void PendSV_Handler(void) __attribute__((weak, alias("DefaultHandler")));
void SysTick_Handler(void) __attribute__((weak, alias("DefaultHandler")));
void WWDG_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void PVM_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void RTC_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void FLASH_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void RCC_CRS_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void EXTI0_1_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void EXTI2_3_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void EXTI4_15_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void USB_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void DMA1_Channel1_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void DMA1_Channel2_3_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void DMAMUX_DMA1_Channel4_5_6_7_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void ADC_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM1_BRK_UP_TRG_COM_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM1_CC_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM2_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM3_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM14_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM15_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM16_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void TIM17_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void I2C1_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void I2C2_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void SPI1_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void SPI2_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void USART1_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void USART2_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void USART3_4_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void FDCAN_IT0_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));
void FDCAN_IT1_IRQHandler(void) __attribute__((weak, alias("DefaultHandler")));

const isr_handler_t __vector_table[] __attribute__((section(".isr_vector"))) = {
    (isr_handler_t)&top_of_stack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    SVCall_Handler,
    0,
    0,                 
    PendSV_Handler,
    SysTick_Handler,
    WWDG_IRQHandler,
    PVM_IRQHandler,
    RTC_IRQHandler,
    FLASH_IRQHandler,
    RCC_CRS_IRQHandler,
    EXTI0_1_IRQHandler,
    EXTI2_3_IRQHandler,
    EXTI4_15_IRQHandler,
    USB_IRQHandler,
    DMA1_Channel1_IRQHandler,
    DMA1_Channel2_3_IRQHandler,
    DMAMUX_DMA1_Channel4_5_6_7_IRQHandler,
    ADC_IRQHandler,
    TIM1_BRK_UP_TRG_COM_IRQHandler,
    TIM1_CC_IRQHandler,
    TIM2_IRQHandler,
    TIM3_IRQHandler,
    0,
    0,       
    TIM14_IRQHandler,
    TIM15_IRQHandler,
    TIM16_IRQHandler,
    TIM17_IRQHandler,
    I2C1_IRQHandler,
    I2C2_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    USART3_4_IRQHandler,
    FDCAN_IT0_IRQHandler,
    FDCAN_IT1_IRQHandler,
}; 

