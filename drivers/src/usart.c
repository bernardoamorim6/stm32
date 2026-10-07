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
    /* BRR is only writable while the USART is disabled, so clear UE first.
       Clearing it also resets every status flag in ISR, which is what we want
       at init time. */
    usart->CR1 = 0;

    /* With OVER8 = 0 the divisor is just the clock over the baud rate. The
       + baud / 2 rounds to nearest instead of truncating, which halves the
       worst case baud error. At 12 MHz and 115200 this gives 104, for a real
       rate of 115384 and an error of +0.16%, well inside what a UART
       tolerates. */
    usart->BRR = (pclk_hz + baud / 2) / baud;

    /* Transmitter and receiver on. RXNEIE is on from the start because a byte
       can arrive at any moment. TXEIE is deliberately left off: TXE is set
       almost all the time, so an always-enabled TX interrupt would fire
       continuously with nothing to send. It is switched on per write and
       switched off again by the handler when the ring runs dry. */
    usart->CR1 = make_mask(USART_CR1_TE, 1)
           | make_mask(USART_CR1_RE, 1)
           | make_mask(USART_CR1_RXNEIE, 1);
    usart->CR1 = set_bits(usart->CR1, make_mask(USART_CR1_UE, 1));
}

/* CR1 holds UE, TE, RE and RXNEIE alongside TXEIE, so setting one bit is a
   load, an or and a store over shared state that the handler also writes.
   Cortex-M0+ has no LDREX/STREX, so masking interrupts is the only way to make
   those three instructions atomic. Kept to exactly one register access, since
   every instruction in here adds to worst case interrupt latency. */
static void tx_start(USART_Regs *usart){
    uint32_t state = critical_enter();
    usart->CR1 = set_bits(usart->CR1, make_mask(USART_CR1_TXEIE, 1));
    critical_exit(state);
}

void uart_write_byte(USART_Regs *usart, uint8_t byte){
    /* Spin until the ring has room. Re-asserting TXEIE on every pass is belt
       and braces: a full ring is by definition non-empty, so the invariant
       "ring non-empty implies TXEIE set" says it must already be on. If this
       ever rescues anything, the invariant is broken somewhere else. */
    while (!rb_put(&tx_ring, byte)) {
        tx_start(usart);
    }
    tx_start(usart);
}

bool uart_read_byte(uint8_t *out){
    /* No critical section, each index has exactly one writer and
       a single aligned 32-bit load or store cannot be seen half done. */
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

    /* A byte arrived before the previous one was read. That byte is lost
       either way, but reception stays jammed until ORE is cleared, so without
       this the RX path dies permanently the first time it falls behind. ORE is
       one of the few flags actually cleared by writing to ICR. */
    if (extract(status, USART_ISR_ORE, 1) == 1) {
        USART2->ICR = make_mask(USART_ICR_ORECF, 1);
    }

    if (extract(status, USART_ISR_RXNE, 1) == 1) {
        /* Reading RDR is what clears RXNE, so the read has to happen even if
           the ring is full, or this handler re-enters forever. A full ring
           drops the byte, which is the right trade for a receive path with
           nowhere to push back to. */
        uint8_t byte = (uint8_t)USART2->RDR;
        rb_put(&rx_ring, byte);
    }

    /* TXE is set whenever TDR is free, which is most of the time, so it says
       nothing about whether there is work to do. TXEIE is what marks this
       interrupt as ours to service. */
    if (extract(status, USART_ISR_TXE, 1) == 1 &&
        extract(USART2->CR1, USART_CR1_TXEIE, 1) == 1) {
        uint8_t byte;
        if (rb_get(&tx_ring, &byte)) {
            USART2->TDR = byte;   /* writing TDR is what clears TXE */
        } else {
            /* Ring empty. Switch the interrupt off, or it re-fires forever
               with nothing to send and starves everything else. */
            USART2->CR1 = clear_bits(USART2->CR1, make_mask(USART_CR1_TXEIE, 1));
        }
    }
}
