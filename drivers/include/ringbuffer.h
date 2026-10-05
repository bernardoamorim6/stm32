#ifndef RINGBUFFER_H   
#define RINGBUFFER_H 

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RBSIZE 64

typedef struct {
    uint8_t  arr[RBSIZE];           /*!< Ring buffer, array length 64, usable capacity 63      */
    uint8_t volatile head;          /*!< Ring buffer head   */
    uint8_t volatile tail;          /*!< Ring buffer tail   */
} RingBuffer;

bool rb_is_empty(RingBuffer *ring);
bool rb_is_full(RingBuffer *ring);
bool rb_put(RingBuffer *ring, uint8_t value);
bool rb_get(RingBuffer *ring, uint8_t *out);



#ifdef __cplusplus
}
#endif


#endif //RINGBUFFER_H