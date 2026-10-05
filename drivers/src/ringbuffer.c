#include "../include/ringbuffer.h"
#include <stdint.h>

bool rb_is_empty(RingBuffer *ring){
    if (ring->tail == ring->head) {
        return true;
    }
    return false;
}

bool rb_is_full(RingBuffer *ring){
    if (((ring->head + 1) & (RBSIZE - 1)) == ring->tail) {
        return true;
    }
    return false;
}

bool rb_put(RingBuffer *ring, uint8_t value){
    if (rb_is_full(ring)) {
        return false;
    }
    ring->arr[ring->head] = value;
    ring->head = (ring->head + 1) & (RBSIZE - 1);
    return true;
}

bool rb_get(RingBuffer *ring, uint8_t *out){
    if (rb_is_empty(ring)) {
        return false;
    }
    *out = ring->arr[ring->tail];
    ring->tail = (ring->tail + 1) & (RBSIZE - 1);
    return true;
}