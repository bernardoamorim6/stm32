#ifndef BITOPS_H   
#define BITOPS_H 

#include <stdint.h>

// Creates a mask of 'width' ones starting at 'position'
static inline uint32_t make_mask(uint32_t position, uint32_t width) {
    if (width == 0) return 0;
    // Handle edge case where width is 32 to prevent undefined behavior
    if (width >= 32) return 0xFFFFFFFFU; 
    
    uint32_t mask = (1U << width) - 1;
    return mask << position;
}

#endif