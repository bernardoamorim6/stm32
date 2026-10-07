#ifndef USART_H   
#define USART_H 

#include "stm32c031_regs.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit positions, from RM0490's USART register descriptions. Only the ones this
   driver touches are listed. Everything else 8N1 needs (M1:M0 = 00 for eight
   data bits, PCE = 0 for no parity, CR2's STOP = 00 for one stop bit, and
   OVER8 = 0 for oversampling by 16) is already the reset value, so the driver
   leaves those fields alone. */
#define USART_CR1_UE 0U       /* USART enable, the master switch */
#define USART_CR1_TE 3U       /* Transmitter enable */
#define USART_CR1_TXEIE 7U    /* Enables the interrupt that fires when the transmitter is free */
#define USART_CR1_RE 2U       /* Receiver enable */
#define USART_CR1_RXNEIE 5U   /* Enables the interrupt that fires on arrival */
#define USART_ISR_RXNE 5U     /* Not empty, a byte is waiting */


#define USART_ISR_TXE 7U      /* Transmit data register empty, 1 means free */
#define USART_ISR_ORE 3U      /* Over run Error */
#define USART_ICR_ORECF 3U    /* Write 1 here to clear ORE. RXNE and TXE are not cleared this way */

/* Configures baud rate and enables the transmitter.
   pclk_hz is the peripheral clock actually feeding the USART, which on this
   part is always PCLK because USART2 has no kernel clock mux. Does not touch
   RCC or GPIO: the caller enables the peripheral clock and puts the pins in
   alternate function mode before calling this. */
void uart_init(USART_Regs *usart, uint32_t pclk_hz, uint32_t baud);

/* Queues one byte for transmission and makes sure the TX interrupt is on.
   Blocking: if the ring is full it spins until the ISR has drained a slot, so
   nothing is ever silently dropped. The bound is the time to send 63 bytes,
   about 5.5 ms at 115200.

   IMPORTANT: DON'T call this from an ISR, and never with interrupts disabled. It waits
   for USART2_IRQHandler to make room, so if the handler cannot run it never
   returns. */
void uart_write_byte(USART_Regs *usart, uint8_t byte);

/* Takes one byte out of the receive ring. Non-blocking: returns false and
   leaves *out untouched when nothing has arrived. */
bool uart_read_byte(uint8_t *out);

/* Writes bytes until the terminating null. The null itself is not sent. */
void uart_write_string(USART_Regs *usart, const char *str);

#ifdef __cplusplus
}
#endif


#endif //USART_H
