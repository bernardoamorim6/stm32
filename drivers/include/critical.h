#ifndef CRITICAL_H   
#define CRITICAL_H 

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cortex-M0+ has no LDREX/STREX, so there is no atomic read-modify-write
   instruction. Masking interrupts is the only way to make a multi-instruction
   sequence uninterruptible. PRIMASK is a single bit inside the core: 1 means
   all maskable interrupts are held off, 0 means they are accepted. It is not
   memory-mapped, so it can only be reached with MRS and MSR.

   These save and restore rather than disable and enable, so that a critical
   section nested inside another one does not hand interrupts back early. */

static inline uint32_t critical_enter(void){
    uint32_t primask;
    __asm__ volatile ("mrs %0, PRIMASK" : "=r" (primask));
    __asm__ volatile ("cpsid i" ::: "memory");
    return primask;
}

static inline void critical_exit(uint32_t state){
    __asm__ volatile ("msr PRIMASK, %0" :: "r" (state) : "memory");
}

#ifdef __cplusplus
}
#endif


#endif //CRITICAL_H
