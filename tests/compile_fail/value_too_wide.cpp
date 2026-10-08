// Must not compile: 4 needs three bits and the field has two.
#include "reg.hpp"
struct Fake {
    static std::uint32_t read(std::uintptr_t) { return 0; }
    static void write(std::uintptr_t, std::uint32_t) {}
};
int main() { hal::Field<hal::Reg<0, Fake>, 10, 2>::write<4>(); }
