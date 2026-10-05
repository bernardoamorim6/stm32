#ifndef USART_H   
#define USART_H 

#include "stm32c031_regs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit positions, from RM0490's USART register descriptions. Only the ones this
   driver touches are listed. Everything else 8N1 needs (M1:M0 = 00 for eight
   data bits, PCE = 0 for no parity, CR2's STOP = 00 for one stop bit, and
   OVER8 = 0 for oversampling by 16) is already the reset value, so the driver
   leaves those fields alone. */
#define USART_CR1_UE   0U   /* USART enable, the master switch */
#define USART_CR1_TE   3U   /* Transmitter enable */
#define USART_ISR_TXE  7U   /* Transmit data register empty, 1 means free */

/* Configures baud rate and enables the transmitter.
   pclk_hz is the peripheral clock actually feeding the USART, which on this
   part is always PCLK because USART2 has no kernel clock mux. Does not touch
   RCC or GPIO: the caller enables the peripheral clock and puts the pins in
   alternate function mode before calling this. */
void uart_init(USART_Regs *usart, uint32_t pclk_hz, uint32_t baud);

/* Blocks until the transmit data register is free, then writes one byte. */
void uart_write_byte(USART_Regs *usart, uint8_t byte);

/* Writes bytes until the terminating null. The null itself is not sent. */
void uart_write_string(USART_Regs *usart, const char *str);

#ifdef __cplusplus
}
#endif


#endif //USART_H
