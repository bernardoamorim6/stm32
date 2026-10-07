#ifndef USART_H   
#define USART_H 

#include "stm32c031_regs.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit positions from RM0490, only the ones this driver touches. 8N1 at
   oversample 16 is the reset state of M1:M0, PCE, STOP and OVER8, so those are
   left alone rather than restated. */
#define USART_CR1_UE 0U       /* USART enable, the master switch */
#define USART_CR1_TE 3U       /* Transmitter enable */
#define USART_CR1_TXEIE 7U    /* Enables the interrupt that fires when the transmitter is free */
#define USART_CR1_RE 2U       /* Receiver enable */
#define USART_CR1_RXNEIE 5U   /* Enables the interrupt that fires on arrival */
#define USART_ISR_RXNE 5U     /* Not empty, a byte is waiting */


#define USART_ISR_TXE 7U      /* Transmit data register empty, 1 means free */
#define USART_ISR_ORE 3U      /* Over run Error */
#define USART_ICR_ORECF 3U    /* Write 1 here to clear ORE. RXNE and TXE are not cleared this way */

/* pclk_hz is the clock feeding the USART, always PCLK here since USART2 has no
   kernel clock mux. Does not touch RCC, GPIO or the NVIC: the caller enables
   the peripheral clock and configures the pins first. */
void uart_init(USART_Regs *usart, uint32_t pclk_hz, uint32_t baud);

/* Queues one byte for transmission and makes sure the TX interrupt is on.
   Blocking: if the ring is full it spins until the ISR has drained a slot, so
   nothing is ever silently dropped. The bound is the time to send 63 bytes,
   about 5.5 ms at 115200.

   IMPORTANT: DON'T call this from an ISR, and never with interrupts disabled. It waits
   for USART2_IRQHandler to make room, so if the handler cannot run it never
   returns. */
void uart_write_byte(USART_Regs *usart, uint8_t byte);

/* Non-blocking. Returns false and leaves *out untouched when nothing has
   arrived. */
bool uart_read_byte(uint8_t *out);

/* Writes bytes until the terminating null. The null itself is not sent. */
void uart_write_string(USART_Regs *usart, const char *str);

#ifdef __cplusplus
}
#endif


#endif //USART_H
