#include "../include/usart.h"
#include "../include/stm32c031_regs.h"
#include "../include/bitops.h"
#include <stdint.h>


void uart_init(USART_Regs *usart, uint32_t pclk_hz, uint32_t baud){
    /* BRR is only writable while the USART is disabled, so clear UE first.
       Clearing it also resets every status flag in ISR, which is what we want
       at init time. */
    usart->CR1 = insert(usart->CR1, 0, USART_CR1_UE, 1);

    /* With OVER8 = 0 the divisor is just the clock over the baud rate. The
       + baud / 2 rounds to nearest instead of truncating, which halves the
       worst case baud error. At 12 MHz and 115200 this gives 104, for a real
       rate of 115384 and an error of +0.16%, well inside what a UART
       tolerates. */
    usart->BRR = (pclk_hz + baud / 2) / baud;

    /* Transmitter on, then the master enable last. */
    usart->CR1 = insert(usart->CR1, 1, USART_CR1_TE, 1);
    usart->CR1 = insert(usart->CR1, 1, USART_CR1_UE, 1);
}

void uart_write_byte(USART_Regs *usart, uint8_t byte){
    /* TXE is set by hardware once TDR has been copied into the shift register,
       meaning TDR is free again. Writing before that would overwrite a byte
       still waiting to go out. */
    while (extract(usart->ISR, USART_ISR_TXE, 1) == 0) {
    }
    usart->TDR = byte;
}

void uart_write_string(USART_Regs *usart, const char *str){
    while (*str != '\0') {
        uart_write_byte(usart, (uint8_t)*str);
        str++;
    }
}
