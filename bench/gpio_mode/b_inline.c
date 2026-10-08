#include "stm32c031_regs.h"
#include "bitops.h"
void set_pa5_output(void) { GPIOA->MODER = insert(GPIOA->MODER, 1, 10, 2); }
