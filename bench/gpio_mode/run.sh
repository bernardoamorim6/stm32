#!/usr/bin/env bash
# Same operation three ways: PA5 to output mode, built at -O2 for the target.
#   A  the C driver as used:   gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT)
#   B  C, inline constants:    insert() on GPIOA->MODER
#   C  the C++ template:       Field<GpioaModer, 10, 2>::write<1>()
# B against C is the abstraction cost. A against C is the practical comparison.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

f="-O2 -mcpu=cortex-m0plus -mthumb -I$root/drivers/include -I$root/hal/include"
arm-none-eabi-gcc $f -std=c17 -c "$here/a_driver.c"   -o "$out/a.o"
arm-none-eabi-gcc $f -std=c17 -c "$here/b_inline.c"   -o "$out/b.o"
arm-none-eabi-g++ $f -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
                     -c "$here/c_template.cpp" -o "$out/c.o"
arm-none-eabi-gcc $f -std=c17 -c "$root/drivers/src/gpio.c" -o "$out/gpio.o"

size_of() { printf "%d" "0x$(arm-none-eabi-nm -S "$1" | awk -v s="$2" '$4==s {print $2}')"; }
a=$(size_of "$out/a.o" set_pa5_output); g=$(size_of "$out/gpio.o" gpio_set_mode)
b=$(size_of "$out/b.o" set_pa5_output); c=$(size_of "$out/c.o" set_pa5_output)

printf "A  C driver   %3d bytes  (%d call site + %d gpio_set_mode)\n" $((a + g)) "$a" "$g"
printf "B  C inline   %3d bytes\n" "$b"
printf "C  template   %3d bytes\n" "$c"

for x in b c; do arm-none-eabi-objcopy -O binary --only-section=.text "$out/$x.o" "$out/$x.bin"; done
if cmp -s "$out/b.bin" "$out/c.bin"; then
    echo "B and C are byte-for-byte identical"
else
    echo "B and C differ"; exit 1
fi
