#include "../include/usart.h"
#include "../include/stm32c031_regs.h"
#include "../include/bitops.h"
#include "critical.h"
#include "ringbuffer.h"
#include <stdint.h>

/* The ISR takes no arguments, so these cannot be passed in. */
static RingBuffer tx_ring;
static RingBuffer rx_ring;

void uart_init(USART_Regs *usart, uint32_t pclk_hz, uint32_t baud){
    /* Configure then enable. Assigning rather than read-modify-write also puts
       M1:M0, PCE and OVER8 back to the zeros that mean 8N1 at oversample 16. */
    usart->CR1 = 0;

    /* OVER8 = 0, so the divisor is just clock / baud, rounded to nearest.
       104 at 12 MHz and 115200, a real rate of 115384, +0.16%. */
    usart->BRR = (pclk_hz + baud / 2) / baud;

    /* TXEIE is deliberately absent: TXE is set almost permanently, so leaving
       it on would re-enter the handler forever. uart_write_byte turns it on. */
    usart->CR1 = make_mask(USART_CR1_TE, 1)
           | make_mask(USART_CR1_RE, 1)
           | make_mask(USART_CR1_RXNEIE, 1);
    usart->CR1 = set_bits(usart->CR1, make_mask(USART_CR1_UE, 1));
}

/* Setting one bit of CR1 is a load, an or and a store, and the handler writes
   CR1 too. No BSRR here and no LDREX on M0+, so masking is the only option.
   One access only: time spent masked is added interrupt latency. */
static void tx_start(USART_Regs *usart){
    uint32_t state = critical_enter();
    usart->CR1 = set_bits(usart->CR1, make_mask(USART_CR1_TXEIE, 1));
    critical_exit(state);
}

void uart_write_byte(USART_Regs *usart, uint8_t byte){
    /* Blocks rather than dropping. Re-asserting TXEIE each pass is belt and
       braces; if it ever helps, the "non-empty implies TXEIE" invariant broke. */
    while (!rb_put(&tx_ring, byte)) {
        tx_start(usart);
    }
    tx_start(usart);
}

bool uart_read_byte(uint8_t *out){
    /* No lock: the handler owns head, this owns tail, and an aligned 32-bit
       access is one instruction. */
    return rb_get(&rx_ring, out);
}

void uart_write_string(USART_Regs *usart, const char *str){
    while (*str != '\0') {
        uart_write_byte(usart, (uint8_t)*str);
        str++;
    }
}

void USART2_IRQHandler(void){
    uint32_t status = USART2->ISR;

    /* Overrun jams reception until cleared, so skipping this kills the RX path
       permanently the first time it falls behind. */
    if (extract(status, USART_ISR_ORE, 1) == 1) {
        USART2->ICR = make_mask(USART_ICR_ORECF, 1);
    }

    if (extract(status, USART_ISR_RXNE, 1) == 1) {
        /* Reading RDR clears RXNE, so it must happen even when the ring is
           full and the byte is about to be dropped. */
        uint8_t byte = (uint8_t)USART2->RDR;
        rb_put(&rx_ring, byte);
    }

    /* TXE is set most of the time, so it says nothing about pending work.
       TXEIE is what marks this interrupt as ours. */
    if (extract(status, USART_ISR_TXE, 1) == 1 &&
        extract(USART2->CR1, USART_CR1_TXEIE, 1) == 1) {
        uint8_t byte;
        if (rb_get(&tx_ring, &byte)) {
            USART2->TDR = byte;   /* writing TDR is what clears TXE */
        } else {
            /* Nothing left: switch the interrupt off or it re-fires forever. */
            USART2->CR1 = clear_bits(USART2->CR1, make_mask(USART_CR1_TXEIE, 1));
        }
    }
}
