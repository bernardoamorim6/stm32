// Must not compile: a 2-bit field at position 31 would need bit 32.
#include "reg.hpp"
struct Fake {
    static std::uint32_t read(std::uintptr_t) { return 0; }
    static void write(std::uintptr_t, std::uint32_t) {}
};
int main() { return static_cast<int>(hal::Field<hal::Reg<0, Fake>, 31, 2>::read()); }
