# Project notes

Command reference, the answers I worked out from the reference manuals, and a log of the mistakes I made along the way and what fixed each one. Kept for future reference, since most of them are easy to repeat by accident.

---

# Commands

## Host build (unit tests)

Runs on the laptop's own compiler. This is the only build that runs tests since GoogleTest can't run on a bare-metal target.

```bash
cmake -S . -B build                 # configure
cmake --build build                 # compile
ctest --test-dir build --output-on-failure   # run the tests
```

After editing only source files (not `CMakeLists.txt`), the configure step can be skipped.

## Bare-metal build (ARM firmware)

Cross-compiles for the STM32C031C6 with `arm-none-eabi-gcc`. Produces `build-arm/firmware.elf`. Needs its own build directory because the compiler is fixed at configure time.

```bash
cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-arm
```

No tests here since nothing to run without hardware. `CMAKE_BUILD_TYPE=Debug` matters for stepping: without it there is no `-g`, so no `.debug_line` section, so the debugger shows bare addresses instead of source.

## Flashing

```bash
STM32_Programmer_CLI -c port=SWD -w build-arm/firmware.elf -rst
```

The `.elf` extension is load-bearing: the programmer picks its parser from the extension, which is why `CMakeLists.txt` sets `set_target_properties(firmware PROPERTIES SUFFIX ".elf")`.

## Inspecting the firmware

```bash
arm-none-eabi-objdump -h build-arm/firmware.elf   # section sizes and addresses
arm-none-eabi-nm -n build-arm/firmware.elf        # symbols, sorted by address
arm-none-eabi-size build-arm/firmware.elf         # Flash and SRAM totals
```

What to look for in `objdump -h`:
- `.isr_vector` at VMA `0x08000000`: the vector table must be the first thing in Flash, or the CPU won't find the initial stack pointer and reset vector on boot.
- `.data` with *different* VMA and LMA (`0x20000000` vs somewhere in Flash): it runs from SRAM but is stored in Flash. Reconciling those two addressesis exactly what `Reset_Handler`'s copy loop is for.

What to look for in `nm -n` (`-n` sorts by address, far more useful than the
default alphabetical):
- `__vector_table` at `0x08000000`.
- Every weak handler (`W`) sharing one address with `DefaultHandler` (`T`), that's the `weak, alias(...)` pattern working; 30+ names, one function.
- `top_of_stack` at `0x20003000` (SRAM origin + 12K).
- `_la_data` matching `.data`'s LMA from `objdump`.

## Linting

Both tools need `compile_commands.json`, which the host configure step generates (via `CMAKE_EXPORT_COMPILE_COMMANDS`).

```bash
clang-tidy -p build app/main.c tests/bitops_tests.cpp drivers/src/gpio.c tests/gpio_tests.cpp drivers/src/usart.c
cppcheck --enable=all drivers/ app/
```

## Clean rebuild

Deleting the build directory is the fix for stale-cache weirdness: CMake bakes the compiler choice into the cache at configure time, so a stale directory can silently keep using the wrong one.

```bash
rm -rf build && cmake -S . -B build
rm -rf build-arm && cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
```

---

# What happens from reset to main?

On a Cortex-M, reset is handled **by hardware before any code runs**. The core reads two 32-bit words from address `0x00000000`: the first is loaded into the main stack pointer, the second into the program counter. On STM32, the boot configuration aliases main Flash to `0x00000000`, so those words physically come from `0x08000000`, which is why the linker script places `.isr_vector` there.

That second word is the address of `Reset_Handler`, with bit 0 set to mark Thumb state. Only now does the first instruction execute, and it does so with a valid stack already in place.

`Reset_Handler`'s job is to establish the C runtime environment, because C's guarantees aren't true yet. It copies `.data` from its load address in Flash to its runtime address in SRAM, initialized globals are *stored* in Flash but must *live* in writable memory. It zeroes `.bss`, because the C standard requires static-storage objects with no initializer to start at zero. In C++ it would also walk `.init_array` to run global constructors. Many implementations also call `SystemInit()` here to configure clocks before `main`.

Then it calls `main()`. If `main` ever returns there's nowhere to return to, so the handler traps in an infinite loop.

## Follow-up questions

**"You said `0x00000000`, but your table is at `0x08000000`. Which is it?"**
Both. The core's fetch address is  rchitecturally fixed at `0x00000000`; STM32 aliases a selectable memory into that region based on the boot pin and option bytes. Booting from main Flash, `0x00000000` is a window onto `0x08000000`.

**"Can the table move after boot?"**
Yes, via `VTOR` in the System Control Block. It's zero at reset, so it can't affect the initial fetch, but software can point it elsewhere afterward, which is how bootloaders hand off to an application at a different offset. (Optional in ARMv6-M, always present from Cortex-M3 up.)

**"Why is the reset vector's value odd?"**
Bit 0 is the Thumb bit. Cortex-M executes Thumb only, so every vector entry has it set; the core clears it when loading PC. In my build, `Reset_Handler` is at `0x08000218` and the vector holds `0x08000219`.

**"What if that bit were clear?"**
Immediate HardFault. The core interprets a clear bit 0 as an attempt to enter ARM state, which ARMv6-M/v7-M don't support.

**"How does the core find the handler for IRQ 12?"**
Pure arithmetic: `table_base + 4 × exception_number`. No searching. That's why reserved slots must still occupy their word, omitting one shifts every later
entry and silently misroutes interrupts.

**"Why `KEEP()` around `.isr_vector` in the linker script?"**
Nothing in the C code references the table by symbol, only hardware reads it. With `--gc-sections`, the linker would see an unreferenced section and discard it. `KEEP` prevents that.

**"LMA versus VMA?"**
Load address versus runtime address. `.text` has them equal: it executes from Flash. `.data` has them differ: LMA in Flash, VMA in SRAM. That gap is precisely what the copy loop closes.

**"Why does `int x = 0;` end up in `.bss`, not `.data`?"**
Because `.data` costs Flash, every byte must be stored in the image. Zero is exactly what the `.bss` loop produces for free, so storing a literal zero in Flash to copy over memory that's about to be zeroed is pure waste. Only non-zero initializers go in `.data`.

**"What breaks if you skip the `.bss` zeroing?"**
Uninitialized globals hold whatever was in SRAM, often stale data from before a warm reset. Code appears to work, then fails on a cold boot, or after an unrelated change shifts the memory layout.

---

# Where does my clock come from?

Worked out from RM0490, RCC chapter: no tutorial, no HAL, just the document.

## What is the core running on immediately after reset, and why?

The HSISYS is used as system clock source after startup from reset, with the division by four (producing 12 MHz frequency). The system clock maximum frequency is 48 MHz. Upon system reset, the HSISYS clock derived from HSI48 oscillator is selected as system clock. When a clock source is used as a system clock, it is not possible to stop it.

## The oscillator

HSI48 oscillator. The HSI48 clock signal is generated from an internal 48 MHz RC oscillator. The system clock maximum frequency is 48 MHz. `HSION` (bit 8) enables it and `HSIRDY` (bit 10) reports it stable, both in `RCC_CR`.

## Dividers between the oscillator and the core

```
HSI48 48 MHz
  └─ HSIDIV  [CR 13:11]   ÷4  → HSISYS 12 MHz
      └─ SW  [CFGR 2:0]   selects HSISYS
          └─ SYSDIV [CR 4:2]   ÷1  → SYSCLK 12 MHz
              └─ HPRE [CFGR 11:8] ÷1 → HCLK 12 MHz ← core
```

## The register fields that control all of it

Only the fields in the oscillator-to-core path are listed. The other ~20 RCC registers are peripheral clock enables (`RCC_IOPENR` and friends), reset control, low-power gating and interrupt flags, none of them answer "where does the core clock come from".

| Register | Field | Bits | Reset value | Meaning |
|---|---|---|---|---|
| `RCC_CR` | `HSION` | 8 | `1` | HSI48 oscillator enabled |
| `RCC_CR` | `HSIRDY` | 10 | `1` | Read-only flag: HSI48 stable and usable |
| `RCC_CR` | `HSIDIV[2:0]` | 13:11 | `010` = ÷4 | Divides HSI48 to produce HSISYS. `000`=÷1 … `111`=÷128 |
| `RCC_CFGR` | `SW[2:0]` | 2:0 | `000` | SYSCLK source select. `000`=HSISYS, `001`=HSE, `011`=LSI, `100`=LSE |
| `RCC_CFGR` | `SWS[2:0]` | 5:3 | `000` | Read-only: which source is *actually* active. Poll this after writing `SW` |
| `RCC_CFGR` | `HPRE[3:0]` | 11:8 | `0xxx` = ÷1 | AHB prescaler: SYSCLK → HCLK (the core clock). `1000`=÷2 … `1111`=÷512 |
| `RCC_CFGR` | `PPRE[2:0]` | 14:12 | `0xx` = ÷1 | APB prescaler: HCLK → PCLK (peripheral bus) |

`SW` versus `SWS` is the pattern worth remembering: you *request* a clock source by writing `SW`, and the hardware confirms the switch actually happened by updating `SWS`. Switching clocks means writing one and polling the other until they agree, the new source has to be stable before the core will run on it.

## The clock path, end to end

HSI48 RC 48 MHz clock osurce, to the RCC_CR HSIDIV[2:0] = 010 box, output labelled HSISYS. That box is HSIDIV, at reset ÷4, so 12 MHz. This goes into the mux (RCC_CFGR SW[2:0] = 000) in the middle. That's SW, at reset selecting HSISYS. IT then goes to SYSDIV(3)(SYSDIV(3)	RCC_CR SYSDIV[2:0] = 000) output labelled SYSCLK, at reset ÷1, still 12 MHz.It then goes to AHB PRESC (AHB PRESC	RCC_CFGR HPRE[3:0] = 0xxx) with output HCLK, at reset ÷1, still 12 MHz. Finally it goes to core

## Changing it deliberately

Changed RCC_CR bits [13:11] from 0b010 to 0b011, which makes the diviser go to 8 (12MHz to 6MHz), which halves the clock speed, making the LED on the board link noticeably slower.

## Flash wait states, if I ever speed it up

RM0490 Table 14: HCLK ≤ 24 MHz needs `LATENCY[2:0]` = `000`, HCLK ≤ 48 MHz needs `001`. At reset HCLK is 12 MHz and `LATENCY` is `000`, so going *slower* is always safe. Going faster is not, the order is asymmetric:
- **Increasing HCLK:** set `LATENCY` first, read it back to confirm, *then* change the clock.
- **Decreasing HCLK:** change the clock first, confirm via `SWS`/`HPRE`, *then* lower `LATENCY`.

The principle is that I must always have *at least* enough wait states for the current speed. Add margin before I need it; remove it only after I don't.

---

# SysTick, and what the instruments said

## Configuring it

SysTick is a Cortex-M core peripheral, so it is documented in **PM0223**, not RM0490 since ARM designed it, not ST. Two consequences that cost me time before I understood them:

- It does **not** appear in the peripheral viewer. `svd/STM32C031.svd` describes ST peripherals only, which is the same reason `SCB`/`VTOR` are missing from it. I read it with `x/4xw 0xE000E010` in the debug console instead.
- It lives in the System Control Space at `0xE000E010`, nowhere near the `0x4002xxxx` / `0x5000xxxx` ranges everything else has been in.

Four registers, laid out consecutively, so a struct of four `volatile uint32_t` *is* the hardware layout:

| Offset | Register | |
|---|---|---|
| `0x00` | `CSR` | control and status |
| `0x04` | `RVR` | reload value |
| `0x08` | `CVR` | current value |
| `0x0C` | `CALIB` | calibration |

`CSR` bits: `ENABLE` (0), `TICKINT` (1), `CLKSOURCE` (2), `COUNTFLAG` (16). I set the low three, so: counter running, interrupt on reaching zero, clocked from HCLK. `CLKSOURCE = 0` would instead select HCLK/8, that "to Cortex system timer" branch off Figure 9, and would make the reload arithmetic wrong by 8×.

**Reload value: `HCLK/1000 - 1`, so 11999 at 12 MHz.** The `-1` is not about call overhead. The counter counts down *through* zero: loaded with N it visits N, N-1, … 1, 0, which is N+1 counts. The period is `RVR + 1` cycles, so `RVR = cycles - 1`. It is inclusive-endpoint arithmetic, entirely deterministic.

**Write order matters:** `RVR`, then `CVR`, then `CSR`. Writing *any* value to `CVR` clears both the current count and `COUNTFLAG`. Skip it and the counter starts from whatever was already in the register, making the first tick the wrong length. Enable last, once the period and starting count are right.

24-bit counter, so the maximum reload is `0xFFFFFF` = 16,777,215. 11999 leaves enormous headroom, but at 100 MHz a 1 ms tick would be 99,999 and a 1 *second* tick would not fit at all.

## The handler needs no registration

Defining `SysTick_Handler` in `systick.c` overrides the weak alias in `startup.c` at link time. No table edit, no registration call. `nm` shows it:

```
08000280 T DefaultHandler
080002b0 T SysTick_Handler
```

Before, `SysTick_Handler` was `W` and shared `DefaultHandler`'s address. And the vector table's slot 15 (offset `0x3C`) holds `0x080002b1`, which is the handler's address with the Thumb bit set.

## What the instruments said

Measured on PA5 at the Arduino D13 header, logic analyser and multimeter independently:

| Target | Logic analyser | Multimeter | Deviation |
|---|---|---|---|
| 100 Hz (`delay_ms(5)`) | 100.5 Hz | 100.5 Hz | +0.5% |
| 1 Hz (`delay_ms(500)`) | ~1.005 Hz | ~1.005 Hz | +0.5% |

The DMM struggles at 1 Hz, because most meters need a gate window longer than a full cycle, so readings there are slow and jumpy. Measuring at 100 Hz first got around that: both frequencies come from the same 1 ms tick, so if the tick is right at 100 Hz it is right at 1 Hz.

**Same 0.5% at both frequencies is what identifies the cause.** A firmware
error would be quantised: an off-by-one in the reload is 0.008%, a miscounted tick is 20%. Nothing in this code produces 0.5%; that is 60 cycles out of 12000 and there is no 60 in it. Fixed per-call overhead is ruled out too, because it would be proportionally large at `delay_ms(5)` and negligible at `delay_ms(500)`. The error would shrink between the rows, and it does not.

A time base 0.5% fast explains it and scales everything uniformly. HSI48 is an RC oscillator: 48.24 MHz instead of 48.00 gives 12.06 MHz HCLK, a 0.995 ms tick, a 9.95 ms period.

What cannot drift: interrupt latency. SysTick reloads in hardware the moment the counter hits zero, regardless of when the CPU runs the handler. Late interrupts cause jitter, never accumulating error.

## Is 0.5% actually within spec?

Yes. RM0490's HSI48 section states that each device is factory calibrated to **1 % accuracy at TA=25°C**, with the calibration value loaded into `HSICAL[7:0]` at reset. My +0.5% is half that budget on a room-temperature bench, so the measurement is unremarkable rather than suspicious.

Two qualifications. That 1 % is the accuracy of the factory trim at 25°C specifically, and the same section says voltage and temperature variations may shift the frequency, so it is a best case rather than a guarantee. And it is the reference manual's figure, not the datasheet's: DS13867 carries the full accuracy table across the operating temperature range, which will be wider.

## The error is correctable in software

`RCC_ICSCR`, at offset `0x04` from the RCC base, so `0x40021004`:

| Field | Bits | Access | Purpose |
|---|---|---|---|
| `HSICAL` | 7:0 | read-only | the live calibration value: an invisible factory number plus `HSITRIM` |
| `HSITRIM` | 14:8 | read/write | the only part I can change |

`HSICAL` is not a value I load or write. RM0490 says it is "a sum of an internal factory-programmed number and the value of the `HSITRIM[6:0]` bitfield", so the factory contribution is never directly visible. `HSITRIM` is the whole interface. It resets to `0x40`, which I confirmed by reading the register on the chip rather than trusting the SVD.

RM0490 §6.2.14 also describes measuring HSI48 against a reference using TIM14/TIM16/TIM17, which is the chip's built-in version of what I did externally with a logic analyser.

## Trimming it out

Datasheet Table 41 gives the trimming step as 0.3% typical per code, with a 0.2 to 0.4% range, and flags three discontinuities: 127 to 128 is -6% typical, 63 to 64 and 191 to 192 are -3.8%. Every figure in that table is characterisation data, not production tested, so I stepped the trim and measured rather than calculating a target.

At 100 Hz, where the multimeter is comfortable:

| `HSITRIM` | Measured | Error |
|---|---|---|
| 64 (reset) | 100.5 Hz | +0.5% |
| 65 | 100.8 Hz | +0.8% |
| 63 | 100.2 Hz | +0.3% |
| **62** | **99.94 Hz** | **-0.06%** |

Code 65 confirmed direction and step in one reading: +1 code gave +0.3%, so lower codes were the way down. Two codes below reset moved the frequency 0.557%, about 0.28% per code, matching the datasheet typical closely.

**Stopping at 62 is forced by granularity, not by giving up.** With a 0.28% step, code 63 lands near +0.22% and 62 at -0.06%, and there is nothing between them. The best achievable is roughly half a step from zero, and -0.06% is essentially that. The residual is now smaller than the reading spread on either instrument.

### The discontinuity did not appear, and I had expected it to

I predicted 64 to 63 would jump about +3.8% because the datasheet lists that transition as a discontinuity. It did not. The reason is in the RM's wording: the discontinuities occur at multiples of 64 in **`HSICAL`**, and `HSICAL` is the factory number *plus* `HSITRIM`. The boundaries therefore sit wherever that sum crosses 64, 128 or 192, which depends on a factory value I cannot read. On this chip the response is linear across at least 62 to 65.

That is another argument for measuring instead of computing: the position of the non-linear regions is not knowable in advance from the documentation.

### What this does and does not fix

It cancels *this die's* manufacturing offset at bench temperature. Table 41's `Δ_Temp(HSI)` row still allows ±1% over 0 to 85°C and -2.5/+2% over the full range, and none of that drift is addressed. The honest description is "calibrated at room temperature", not "accurate".

The value 62 is also specific to this chip. Another STM32C031 has a different factory offset and would need a different code, so it belongs in the source as a measured constant with a comment, not as a magic number that looks universal.

---

# Writing register definitions by hand

## The struct is the memory map

A peripheral struct it is the mapping of the hardware. The compiler lays members out consecutively, and overlaying that on a base address encompasses whole mechanism. So member order and size are load-bearing in a way ordinary structs are not: insert one member and every register after it
silently points somewhere else.
Creating a struct for the registers creates a map of the hardware. Since the compiler lays members out consecutively, overlaying them on a bae address encompasses the whole mechanism. This makes size and order load bearing in a way that regular C structs are not, inserting the members in a ceritain order is imperative to make sure they point to the right place.

GPIO is contiguous from `0x00` to `0x28` with no reserved holes, so I didn't need to add any padding members. Other peripherals will probably not be so tidy, and there the gaps will have to be filled with explicit reserved members or everything downstream shifts silently.

Two things I would have got wrong without checking:

- Members must be `volatile`. Without it the compiler may cache a read of `IDR`, drop a write to `ODR` it thinks is redundant, or reorder accesses.
- `AFRL` and `AFRH` are one array, `AFR[2]`. They are split only because 16 pins times 4 bits is 64 bits and will not fit in one 32-bit register.

## Proving the layout at compile time

`static_assert(offsetof(GPIO_Regs, ODR) == 0x14, ...)` for every member, plus one on `sizeof`. This costs nothing at runtime and makes the build fail if anything is ever wrong.

## Bits per pin varies by register, and nothing warns you

This is where most of my mistakes landed. The field position is not `pin` across the board:

| Register | Bits per pin | Position |
|---|---|---|
| `MODER`, `OSPEEDR`, `PUPDR` | 2 | `pin * 2` |
| `OTYPER`, `IDR`, `ODR` | 1 | `pin` |
| `AFR[pin / 8]` | 4 | `(pin % 8) * 4` |

I accidentally used `pin * 2` on `OTYPER`, which is one bit per pin, so configuring pin 5 would have written bits 11:10 and reconfigured pins 10 and 11 instead. It compiles perfectly. The compiler has no idea what these registers mean.

## BSRR exists to avoid read-modify-write

Setting one bit of `ODR` normally takes read, modify, write. If an interrupt touches the same port between the read and the write, the write silently undoes its work.

`BSRR` removes the read. Bits 15:0 set the matching `ODR` bit, bits 31:16 clear it, zeros do nothing, and it is write-only: a read always returns `0x0000`. One store, atomic, nothing to preserve.

That last detail made my first attempt wrong in an interesting way. I wrote `port->BSRR |= 1u << pin`, which reads first. It produced the right answer, because the read returns zero and `0 | x == x`, so it worked for the wrong reason while being unable to clear a pin at all.

## What made the driver testable

Every function takes `GPIO_Regs *port` instead of hard-coding a base address. Firmware passes `GPIOA`; a test passes a zeroed struct on the stack. The driver cannot tell the difference, so the whole C layer becomes testable on the host with no board attached.

The tests assert both that the target field changed and that its neighbours did not. I verified the suite catches real bugs rather than merely passing: reverting `gpio_write` to the swapped set/clear logic failed exactly the two `BSRR` tests and nothing else.

# USART, from registers to picocom

## Two clocks, not one

Every peripheral here has a bus clock, gated by an `xxEN` bit in RCC. Without it the registers are dead: writes vanish, reads return zero. That is the same failure I already hit with `IOPENR` and GPIO.

Some peripherals also have a *kernel* clock, which is what actually drives the internal logic, in a UART's case the baud rate generator. Its source can be selected independently in `RCC_CCIPR`, so a peripheral can keep a stable rate across a system clock change, or keep running in Stop mode.

Two things I found referencing USART2 while hunting for the enable bit were
`RCC_APBENR1->USART2EN` and  RCC_CR->HSIKERON`. They are not alternatives. `HSIKERON` forces HSI48 to stay running so it can feed peripheral kernels, and `USART2EN` is the bus gate. I needed the second one.

It turns out `HSIKERON` cannot apply to USART2 on this part at all. `CCIPR`
has a `USART1SEL` field and no `USART2SEL`, so USART2 has no kernel clock mux and is always fed from PCLK. The SVD's own description of `HSIKERON` claims HSI48 "can only feed USART1, USART2, and I2C1", which contradicts the register list in the same file. That text is family-level boilerplate across the C0 series. The field list for the specific part wins.

The practical consequence is that the UART baud rate is coupled to the system clock. Changing `HSIDIV` later would break serial output as a side effect. USART1 could have been insulated from that.

## ICR is not the status register

I first wrote down `USART_ICR` at `0x20` as the status register. It is the interrupt flag *clear* register. The one to poll is `USART_ISR` at `0x1C`, directly below it. The C in the name is the whole difference and I read past it.

## The register map has no gaps

`0x00` to `0x2C`, twelve consecutive words: CR1, CR2, CR3, BRR, GTPR, RTOR, RQR, ISR, ICR, RDR, TDR, PRESC. So `USART_Regs` needed no reserved padding members, unlike what I was braced for after the warning that most peripherals are less tidy than GPIO.

One oddity: the SVD lists CR1 and ISR *twice* each, as `_enabled` and `_disabled` variants. That is not a duplicate entry. It is the same register with two documented layouts depending on `FIFOEN`, where some bits change meaning and name. FIFO mode is off at reset and this driver leaves it off, so the non-FIFO layout applies. The offsets are identical either way, so the struct is unaffected.

## The configuration is shorter than I expected

Three control registers exist, and only CR1 needed touching. Everything 8N1 requires is already the reset value:

| Field | Register | Reset | Meaning |
|---|---|---|---|
| `M1:M0` | CR1 bits 28, 12 | `00` | 8 data bits |
| `PCE` | CR1 bit 10 | `0` | no parity |
| `STOP` | CR2 bits 13:12 | `00` | 1 stop bit |
| `OVER8` | CR1 bit 15 | `0` | oversample by 16 |

Checking this was not wasted effort even though the answer was "change nothing". `OVER8` in particular decides whether the simple `BRR` formula applies, and `M0` and `M1` are not adjacent bits despite the manual presenting them as a two-bit value, which is the kind of thing that produces output that is almost right.

Writing those fields anyway would have been harmless but dishonest: it would look like configuration when it is restating a default.

## Order matters in two places

`BRR` is only writable while the USART is disabled, so `uart_init` clears `UE` first. Clearing `UE` also resets every status flag in `ISR`, which is fine at init time. Then `TE`, then `UE` last.

At the application level the peripheral clock has to be enabled before any USART or GPIO register write, for the same reason as the `IOPENR` lesson.

## Why `uart_init` takes a clock frequency

```c
void uart_init(USART_Regs *usart, uint32_t pclk_hz, uint32_t baud);
```

Passing the clock rather than hard-coding a divisor keeps the arithmetic in one place and keeps the function testable off-target, the same reason the GPIO functions take a `GPIO_Regs *`. The divisor rounds to nearest, `(pclk_hz + baud / 2) / baud`, instead of truncating, which halves the worst case baud error.

# Mistakes and fixes

## -nostdlib drops libgcc, and Cortex-M0+ cannot divide

Adding the UART driver broke the link:

```
undefined reference to `__aeabi_uidiv'
```

The offending line was `(pclk_hz + baud / 2) / baud` in `uart_init`. Cortex-M0+ has no hardware divide instruction, so a division with runtime operands compiles to a call into libgcc. My link line used `-nostdlib`, which drops libgcc along with libc.

The two are not the same thing. libc is the C standard library, which a bare-metal target genuinely does not want. libgcc holds the compiler's own support routines: division, 64-bit arithmetic, floating point emulation. The compiler emits calls to them on its own initiative, so refusing to link it means ordinary C stops working for reasons that have nothing to do with the standard library.

Fix: `target_link_libraries(firmware PRIVATE bitops gcc)`. No C library, but
the routines the compiler needs are there. It cost about 100 bytes of Flash.

The reason I had not hit this earlier is that every division in the GPIO driver is by a power-of-two constant, `pin / 8` and `pin % 8`, which the compiler turns into shifts and masks with no call at all.


## ARM build: executable vs static library

`app/main.c` was originally wired up as `add_executable(firmware app/main.c)` in the `CMAKE_CROSSCOMPILING` branch. Building it failed at the link step with undefined references to `_exit`, `_close`, `_lseek`, `_read`, `_write`, `_sbrk`: the libc syscall stubs a real embedded target needs from a linker
script and startup code (vector table, reset handler), neither of which existed at that point.

Fix: I built it as `add_library(firmware STATIC app/main.c)` instead. A static library only compiles and archives object code,it never invokes the linker to produce a final binary, so it can't hit this error. This matched the staged approach: prove the cross-compiler/flags/includes work now, defer full linking to when there's a real linker script. Once the linker script existed I switched it back to `add_executable`.

## The SVD disagrees with the reference manual

`svd/STM32C031.svd` gives `RCC_CR` a reset value of `0x00000500`, which puts
`HSIDIV` at `000` (÷1) and would imply the core boots at 48 MHz. That is wrong. RM0490 states `010: 4 (reset value)`, and the manual's own revision history (Rev 4, 10-Jul-2024) records the fix:

> Corrected RCC_CFGR bitmap, LSERDYIE bit name, LSIRDYF bit description,
> **HSIDIV[2:0] and HSIKERDIV[2:0] reset values in the register map**

The SVD predates that correction, and it is not a stale local copy, it is byte-identical to the one CubeCLT 1.22.0 ships, so ST still had the wrong values in it as of June 2026.

**Settled on hardware.** Neither document is the authority; the silicon is. Halted at `Reset_Handler`, before any code touches RCC, I read `RCC_CR` (`0x40021000`) from the debugger:

```
(gdb) x/1xw 0x40021000
0x40021000:  0x00001540
```

Bits 6, 8, 10 and 12 are set, which decodes as:

| Bit(s) | Field | Value | Meaning |
|---|---|---|---|
| 4:2 | `SYSDIV[2:0]` = `000` | ÷1 | system clock divider |
| 6 | `HSIKERDIV[2:0]` = `010` | ÷3 | kernel clock divider |
| 8 | `HSION` | 1 | HSI48 enabled |
| 10 | `HSIRDY` | 1 | HSI48 stable |
| 12 | `HSIDIV[2:0]` = `010` | ÷4 | HSI48 → HSISYS = 12 MHz |

Every field matches RM0490, including *both* fields the revision history names. So the SVD is wrong on two counts, not one.

Takeaway: the SVD stays useful for register layout: addresses, offsets, bit positions, which is what the peripheral viewer needs; but it is a generated artifact. Where it disagrees with the reference manual, the manual wins; and where it matters, I read the register off the chip and settle it.

## A frozen program looks exactly like a dead pin

After writing `systick.c` the LED stopped blinking, and the logic analyser showed the pin permanently high. I went looking at the analyser settings and the wiring. The actual cause was that I never called `systick_init()` from `main`. SysTick was still at its reset state, so the handler never fired, `counter` never incremented, and `delay_ms()` spun forever on its first call. The LED had been switched on by the line before it and stayed there.

Lesson: a pin stuck at a constant level is a *software* symptom at least as often as a hardware one. Before re-probing, check whether the program is still running at all. A breakpoint in the ISR answers it in seconds.

Related: after fixing it, the LED still did not blink, because I had edited the source but not rebuilt and reflashed. Editing a file pushes nothing to the chip.

## A blinking LED hid a wrong register read

My first read-modify-write on `GPIOA_ODR` read the wrong register:

```c
*GPIOA_ODR = insert(*GPIOA_MODER, 1, 5, 1);   /* reads MODER, writes ODR */
```

The LED blinked correctly anyway. Bit 5 was being set and cleared exactly as intended, so the symptom I was looking for, a working blink, was present while the code was wrong. What actually landed in `ODR` was `MODER`'s contents with bit 5 modified, which set roughly 28 other output bits. Harmless only because those pins were still in analog mode and so weren't driving anything.

Lesson: a working output doesn't verify the write. Read the register back.

## A vector table entry is one greater than the function it points to

`nm` reported `Reset_Handler` at `0x08000218`, but the vector table held `0x08000219`. That's the Thumb bit: Cortex-M executes Thumb only, so bit 0 of every vector entry must be set, and the core clears it when loading PC. A cleared bit 0 means an immediate HardFault.

