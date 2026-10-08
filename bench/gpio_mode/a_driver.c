#include "gpio.h"
void set_pa5_output(void) { gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT); }
