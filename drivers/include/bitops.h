#ifndef BITOPS_H   
#define BITOPS_H 

#include <stdint.h>
#include <stdbool.h>

// Creates a mask of 'width' ones starting at 'position'
static inline uint32_t make_mask(uint32_t position, uint32_t width) {
    if (width == 0) return 0;
    // Handle edge case where width is 32 to prevent undefined behavior
    if (width >= 32) return 0xFFFFFFFFU; 
    
    uint32_t mask = (1U << width) - 1;
    return mask << position;
}



// 	Reads a field's value back out
static inline uint32_t extract(uint32_t reg, uint32_t position, uint32_t width) {
    uint32_t mask = make_mask(position, width);
    return (mask & reg) >> position;
}

// Turns bits on
static inline uint32_t set_bits(uint32_t reg, uint32_t mask) {
    // assuming mask looks like 00011000 when you want to turn on the middle bits
    return reg | mask;
}

// Turns bits off
static inline uint32_t clear_bits(uint32_t reg, uint32_t mask) {
    // mask uses the same convention as make_mask/set_bits: 1 = bit to clear
    return reg & ~mask;
}

// Returns whether bits are set
static inline bool test_bits(uint32_t reg, uint32_t mask) {
    if ((reg | mask) == reg) {
        return true;
    }
    return false;
}


// The full read-modify-write
static inline uint32_t insert(uint32_t reg, uint32_t value, uint32_t position, uint32_t width) {
    uint32_t mask = make_mask(position, width);
    uint32_t cleared_reg = clear_bits(reg, mask);
    // Truncate value to width bits first so an out of range value cant spill into whatever field sits next to this one.
    uint32_t shifted_value = (value & make_mask(0, width)) << position;

    return cleared_reg | shifted_value;
}

#endif