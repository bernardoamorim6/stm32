#pragma once
#include <cstdint>

/* Register fields as types. Position and width are template parameters, so an
   out-of-range write is a compile error and the generated code matches the C. */

namespace hal {
    /* constexpr twin of make_mask in bitops.h. C's static inline cannot run at
       compile time, so it cannot feed a static_assert; this can. */
    constexpr std::uint32_t make_mask(std::uint32_t position, std::uint32_t width){
        if (width == 0) {
            return 0;
        }
        // Handle edge case where width is 32 to prevent undefined behavior
        if (width >= 32) {
            return 0xFFFFFFFFU;
        }

        std::uint32_t mask = (1U << width) - 1;
        return mask << position;
    }

    /* The hardware backend. reinterpret_cast is not allowed in constexpr, so the
       address stays an integer until it reaches here. */
    struct Mmio {
        static std::uint32_t read(std::uintptr_t address){
            volatile std::uint32_t* reg = reinterpret_cast<volatile std::uint32_t *>(address);
            return *reg;
        }
        static void write(std::uintptr_t address, std::uint32_t value){
            *reinterpret_cast<volatile std::uint32_t *>(address) = value;
        }
    };

    /* A type, never an object: everything is static, so there is nothing to
       construct before main. Tests swap Backend for a fake over an array. */
    template <std::uintptr_t Address, typename Backend>
    struct Reg {
        static std::uint32_t read(){
            return Backend::read(Address);
        }
        static void write(std::uint32_t value){
            Backend::write(Address, value);
        }
    };

    template <typename Register, std::uint32_t Position, std::uint32_t Width>
    struct Field {
        static_assert((Width > 0) && (Position + Width <= 32),
                      "field must be 1-32 bits wide and end at or before bit 31");

        static constexpr std::uint32_t mask = make_mask(Position, Width);

        static std::uint32_t read() {
            return (Register::read() & mask) >> Position;
        }

        /* Value is a template parameter because a function argument is never a
           constant expression, so static_assert could not see it. */
        template <std::uint32_t Value>
        static void write(){
            static_assert(Value <= make_mask(0, Width), "value does not fit in field");
            Register::write((Register::read() & ~mask) | (Value << Position));
        }
    };
}

// Compile-time tests: if this header compiles, make_mask is right.
static_assert(hal::make_mask(31, 1) == 0x80000000U);
static_assert(hal::make_mask(10, 2) == 0xC00U);
static_assert(hal::make_mask(0, 1) == 0x1U);
static_assert(hal::make_mask(28, 4) == 0xF0000000U);
static_assert(hal::make_mask(0, 32) == 0xFFFFFFFFU);
static_assert(hal::make_mask(5, 0) == 0x0U);
