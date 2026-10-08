#include <algorithm>
#include <gtest/gtest.h>
#include "reg.hpp"

/* Stands in for Mmio: same two functions, but an "address" is an index into an
   array. A Reg built on this never touches real memory, so the templates run
   on the host unchanged. */
struct Fake {
    static inline std::uint32_t mem[8] = {};
    static std::uint32_t read(std::uintptr_t address) { return mem[address]; }
    static void write(std::uintptr_t address, std::uint32_t value) { mem[address] = value; }
};

class RegTest : public ::testing::Test {
protected:
    void SetUp() override { std::fill(std::begin(Fake::mem), std::end(Fake::mem), 0U); }
};

TEST_F(RegTest, ReadReturnsWhatIsAtItsAddress) {
    Fake::mem[3] = 0xDEADBEEFU;
    EXPECT_EQ((hal::Reg<3, Fake>::read()), 0xDEADBEEFU);
}

TEST_F(RegTest, WriteLandsAtItsAddressOnly) {
    hal::Reg<3, Fake>::write(0x12345678U);
    EXPECT_EQ(Fake::mem[3], 0x12345678U);
    EXPECT_EQ(Fake::mem[2], 0U);
    EXPECT_EQ(Fake::mem[4], 0U);
}

TEST_F(RegTest, DifferentAddressesAreDifferentRegisters) {
    using A = hal::Reg<1, Fake>;
    using B = hal::Reg<5, Fake>;
    A::write(0xAAU);
    B::write(0xBBU);
    EXPECT_EQ(A::read(), 0xAAU);
    EXPECT_EQ(B::read(), 0xBBU);
}

/* Field tests use register 0 of the fake. MODER pin 5 (bits 11:10) is the
   running example, matching the C GPIO tests. */
using R = hal::Reg<0, Fake>;
using Pa5Mode = hal::Field<R, 10, 2>;

TEST_F(RegTest, FieldReadExtractsOnlyItsBits) {
    Fake::mem[0] = 0x00000400U;          // bits 11:10 = 01
    EXPECT_EQ(Pa5Mode::read(), 1U);
}

TEST_F(RegTest, FieldReadIgnoresNeighbours) {
    Fake::mem[0] = 0xFFFFF3FFU;          // everything set except bits 11:10
    EXPECT_EQ(Pa5Mode::read(), 0U);
}

TEST_F(RegTest, FieldWriteSetsItsBits) {
    Pa5Mode::write<1>();
    EXPECT_EQ(Fake::mem[0], 0x00000400U);
}

TEST_F(RegTest, FieldWriteLeavesNeighboursAlone) {
    Fake::mem[0] = 0xFFFFFFFFU;
    Pa5Mode::write<1>();
    EXPECT_EQ(Fake::mem[0], 0xFFFFF7FFU);  // only bit 11 cleared
}

TEST_F(RegTest, FieldWriteOverwritesRatherThanOrsIn) {
    Pa5Mode::write<3>();
    Pa5Mode::write<1>();
    EXPECT_EQ(Pa5Mode::read(), 1U);
}

TEST_F(RegTest, FieldAtTheTopOfTheRegister) {
    hal::Field<R, 28, 4>::write<0xAU>();  // AFR pin 7 shape
    EXPECT_EQ(Fake::mem[0], 0xA0000000U);
}

TEST_F(RegTest, FieldSpanningTheWholeRegister) {
    hal::Field<R, 0, 32>::write<0xFFFFFFFFU>();  // the width-32 path
    EXPECT_EQ(Fake::mem[0], 0xFFFFFFFFU);
}
