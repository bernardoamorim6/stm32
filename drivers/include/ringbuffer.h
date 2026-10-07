#ifndef RINGBUFFER_H   
#define RINGBUFFER_H 

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdalign.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RBSIZE 64

typedef struct {
    uint8_t  arr[RBSIZE];           /*!< Ring buffer, array length 64, usable capacity 63      */
    uint32_t volatile head;          /*!< Ring buffer head   */
    uint32_t volatile tail;          /*!< Ring buffer tail   */
} RingBuffer;

/* Wrapping is done with & (RBSIZE - 1) instead of % RBSIZE, because
   Cortex-M0+ has no divide instruction. */
static_assert((RBSIZE & (RBSIZE - 1)) == 0, "RBSIZE must be a power of two for the & (RBSIZE - 1) wrap");
static_assert(RBSIZE >= 2, "RBSIZE must be at least 2: one slot is sacrificed to the empty/full distinction");
static_assert(RBSIZE - 1 <= UINT32_MAX, "an index must be able to hold RBSIZE - 1");

/* The indices are shared between an ISR and main with no lock, which is only
   safe because a single aligned load or store of one is a single instruction
   on Cortex-M0+ and therefore cannot be observed half-done. These checks pin the width, and the alignment. */
static_assert(sizeof(((RingBuffer *)0)->head) == 4, "head must be a single 32-bit word");
static_assert(sizeof(((RingBuffer *)0)->tail) == 4, "tail must be a single 32-bit word");
static_assert(offsetof(RingBuffer, head) % 4 == 0, "head must be 4-byte aligned within the struct");
static_assert(offsetof(RingBuffer, tail) % 4 == 0, "tail must be 4-byte aligned within the struct");
static_assert(alignof(RingBuffer) >= 4, "the struct itself must be at least 4-byte aligned");

bool rb_is_empty(const RingBuffer *ring);
bool rb_is_full(const RingBuffer *ring);
bool rb_put(RingBuffer *ring, uint8_t value);
bool rb_get(RingBuffer *ring, uint8_t *out);



#ifdef __cplusplus
}
#endif


#endif //RINGBUFFER_H