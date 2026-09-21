#include "../drivers/include/bitops.h"

int main(void) {
    while (1) {
        uint32_t mask = make_mask(10, 2);
        (void)mask;
    }
}
