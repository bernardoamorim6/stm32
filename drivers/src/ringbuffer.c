#include "../include/ringbuffer.h"
#include <stdint.h>

bool rb_is_empty(const RingBuffer *ring){
    if (ring->tail == ring->head) {
        return true;
    }
    return false;
}

/* Full is one slot short of head == tail. Sacrificing that slot is what lets
   head == tail mean empty and nothing else, which in turn is what keeps the
   single producer / single consumer case free of any locking. */
bool rb_is_full(const RingBuffer *ring){
    if (((ring->head + 1) & (RBSIZE - 1)) == ring->tail) {
        return true;
    }
    return false;
}

/* Store, then publish the index. The other side treats head as the signal that
   a slot is ready, so advancing first would expose an unwritten slot. */
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