# STM32C031 bare-metal firmware

Bare-metal firmware for the **NUCLEO-C031C6** (STM32C031C6, Cortex-M0+, 32 KB Flash, 12 KB SRAM), written without HAL, without CubeMX, and without vendor startup code.

Every register definition, the linker script, the vector table and the reset handler are written by hand from **RM0490** (STM32C0 reference manual), **PM0223** (Cortex-M0+ programming manual) and the STM32C031 datasheet.

I did use ST's debugger nad flashing tools, I only wanted to use hand written instead of generated code, not refuse vendor tooling.

---

## Status

This is a work in progress. What follows is what actually runs and is tested, not a plan.

### Implemented

**Bit manipulation library** (`drivers/include/bitops.h`) Header-only C, six `static inline` functions for register field access: `make_mask`, `insert`, `extract`, `set_bits`, `clear_bits`, `test_bits`. Covered by 25 host-side GoogleTest cases including the boundary conditions that actually bite: zero width, full 32-bit width, top-bit fields, and overwriting an existing field value (the case that catches a forgotten clear-before-OR).

**Linker script** (`linker/stm32c031c6.ld`) Memory regions taken from the datasheet: Flash at `0x08000000` (32 KB), SRAM at `0x20000000` (12 KB). Defines `.isr_vector`, `.text`, `.data` and `.bss`, the load-address/runtime-address split for `.data`, and `top_of_stack` derived from `ORIGIN(SRAM) + LENGTH(SRAM)`.

**Startup code** (`drivers/src/startup.c`) Full 48-entry vector table transcribed from RM0490's interrupt table, placed at `0x08000000`. Unused handlers resolve to a single `DefaultHandler` through `__attribute__((weak, alias(...)))`, so a real handler defined anywhere else silently overrides the default at link time. `Reset_Handler` copies `.data` from Flash to SRAM, zeroes `.bss`, and calls `main`.

**Blinky** (`app/main.c`) Direct register writes: GPIOA clock enable in `RCC_IOPENR`, PA5 to output mode in `GPIOA_MODER`, toggle via `GPIOA_ODR`, busy-wait delay.

**Clock tree** The reset clock path is documented end to end in
[`NOTES.md`](NOTES.md): HSI48 (48 MHz) -> `HSIDIV` ÷4 -> HSISYS -> `SW` mux ->
`SYSDIV` ÷1 -> SYSCLK -> `HPRE` ÷1 -> HCLK = **12 MHz** at the core, with the controlling register field and bit range for each stage. Verified by reading `RCC_CR` off the chip at the reset halt, and by deliberately changing `HSIDIV` to ÷8 and observing the blink rate halve.

### Not yet implemented

The scope below is the plan, not a promise, it gets revised as I go, and this section is updated alongside each commit.

**C layer (C17), remaining:**

- GPIO input, pull-ups and alternate function (output works)
- SysTick tick and `delay_ms()`
- Interrupt-driven UART with TX/RX ring buffers
- I²C master written from scratch, then a BME280 driver including the compensation maths
- SSD1306 as a second address on the same bus — enough to prove addressing and capture two devices sharing a bus, no display driver

**A convention not yet met.** Peripheral access is currently inline `volatile uint32_t *` pointers in `main.c`. The intended design is structs of `volatile uint32_t` at fixed base addresses, with every driver taking a pointer to its peripheral's register struct — firmware passes the hardware address, tests pass a struct in RAM. That indirection is the whole host-testing strategy for the C layer, and it lands with the first real driver.

**C++17 layer** (`hal/`, empty until the C drivers exist). It exists to answer one question: can modern C++ make register access safer than the C layer at zero cost? Type-safe registers via templates where an illegal write fails at compile time, RAII for I²C transactions so a stop can never be missed on an early return, `-fno-exceptions -fno-rtti`, `std::span` over the C layer's buffers, `enum class` for modes and errors.

**The measurement.** The same I²C register read performed two ways — through the C driver directly, and through the C++ wrapper — comparing `.text` size and cycle counts against a stripped baseline. Table to follow here.

**Deferred until after the October gate:** timer/PWM output and ADC (internal temperature sensor and VREFINT).

**Not attempted:** input capture / encoder mode (no encoder in the kit) and DMA — a genuine gap, and the highest-value addition if time appears later.

---

## Hardware and toolchain

| | |
|---|---|
| Board | NUCLEO-C031C6 (on-board ST-LINK/V2.1) |
| MCU | STM32C031C6 — Cortex-M0+, 32 KB Flash, 12 KB SRAM |
| Toolchain | Arm GNU Toolchain **15.3.Rel1** (`arm-none-eabi-gcc`) |
| Host tests | GoogleTest, fetched by CMake at configure time |
| Flash / debug | STM32CubeCLT — `STM32_Programmer_CLI`, `ST-LINK_gdbserver` |

The toolchain version is pinned to `15.3.Rel1` in [`.github/workflows/ci.yml`](.github/workflows/ci.yml) so CI builds with the same compiler as the development machine.

Current firmware size:

```
   text    data     bss     dec     hex
    664       4       4     672     2a0
```

---

## Building

Two build directories from one source tree. The compiler is fixed at configure time, so each needs its own.

**Host: builds and runs the unit tests:**

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

**ARM: cross-compiles the firmware:**

```bash
cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-arm
```

The split is driven by `if(CMAKE_CROSSCOMPILING)` in the top-level `CMakeLists.txt`; GoogleTest can't build for a bare-metal target, and firmware makes no sense on a laptop, so each branch takes only its half.

**Flashing:**

```bash
STM32_Programmer_CLI -c port=SWD -w build-arm/firmware.elf -rst
```

**Debugging:** `.vscode/launch.json` is configured for Cortex-Debug with ST-LINK, halting at `Reset_Handler` and loading `svd/STM32C031.svd` for the peripheral register view.

---

## Verifying the memory layout

```bash
arm-none-eabi-objdump -h build-arm/firmware.elf
arm-none-eabi-nm -n build-arm/firmware.elf
```

What this should show, and why it matters:

```
0 .isr_vector   000000c0  08000000  08000000
1 .text         000000b4  080000c0  080000c0
2 .data         00000004  20000000  0800027c
```

`.isr_vector` sits at `0x08000000`: the vector table has to be the first thing in Flash or the core can't find the initial stack pointer and reset vector on boot. `.data` has a different VMA (`0x20000000`, SRAM) and LMA (Flash): it runs from RAM but is stored in Flash, and closing that gap is exactly what the copy loop in `Reset_Handler` does.

---

## Repository layout

```
app/                 application entry point
drivers/include/     C headers (bitops.h)
drivers/src/         C sources (startup.c)
hal/                 C++17 layer (empty until the C drivers exist)
tests/               GoogleTest suites, host-only
linker/              linker script
cmake/               ARM toolchain file
svd/                 CMSIS-SVD register descriptions (for the debugger)
.github/workflows/   CI
```

---

## CI

Three jobs on every push and pull request ([`ci.yml`](.github/workflows/ci.yml)):

- **host**: configure, build, run the full `ctest` suite
- **build-arm**: install the pinned toolchain, cross-compile the firmware, and
  report binary size
- **lint**: `clang-tidy` (using the `compile_commands.json` CMake exports) and `cppcheck`

The size report runs `arm-none-eabi-size` and writes it to the GitHub Actions
job summary, so Flash and SRAM usage is visible on every push without digging
through logs. Flash usage is `text + data`; SRAM usage is `data + bss`.

---

## What I got wrong

[`NOTES.md`](NOTES.md) is a running log of the mistakes I made building this and what fixed each one: kept because most of them are easy to repeat. A sample:

- **The SVD disagrees with the reference manual.** ST's own `STM32C031.svd` gives `RCC_CR` a reset value of `0x00000500`, putting `HSIDIV` at ÷1 and implying a 48 MHz boot clock. RM0490 says ÷4, and its revision history records the correction. I read the register off the chip and got `0x00001540` — the manual is right, the SVD is wrong on two fields, and the silicon settles it.
- **A vector table entry is one greater than the function it points to.** `Reset_Handler` is at `0x08000218`; the table holds `0x08000219`. That's the Thumb bit, and a cleared bit 0 means an immediate HardFault.
- **A blinking LED hid a wrong register read.** My first read-modify-write on `ODR` read `MODER` instead. The LED blinked correctly anyway, because bit 5 was still being set and cleared as intended — so the symptom I was watching for was present while the code was wrong.

`NOTES.md` also holds the full clock-path derivation and a written answer to "what happens between reset and `main()`".
