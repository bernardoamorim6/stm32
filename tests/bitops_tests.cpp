#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include "../drivers/include/bitops.h"

TEST(CreateMask, TwoBitFieldAtTen) {
    EXPECT_EQ(make_mask(10, 2), 0xC00u);
}

TEST(CreateMask, SingleBitAtZero) {
    EXPECT_EQ(make_mask(0, 1), 1u);
}

TEST(CreateMask, TopBit) {
    EXPECT_EQ(make_mask(31, 1), 0x80000000u);
}

TEST(CreateMask, FullWidth) {
    EXPECT_EQ(make_mask(0, 32), 0xFFFFFFFFu);
}

TEST(CreateMask, ZeroWidthIsZero) {
    EXPECT_EQ(make_mask(5, 0), 0u);
}

TEST(Extract, FieldAtPositionZero) {
    EXPECT_EQ(extract(0xFFFFFFFF, 0, 2), 0x3u);
}

TEST(Extract, ShiftsFieldDownToBitZero) {
    // Pin 5's MODER field (bits 11:10) holding mode "1" looks like 0x400 in the raw register;
    // extract should hand back the plain value 1, not the still-shifted 0x400.
    EXPECT_EQ(extract(0x00000400, 10, 2), 0x1u);
}

TEST(Extract, FullWidth) {
    EXPECT_EQ(extract(0xFFFFFFFF, 0, 32), 0xFFFFFFFFu);
}

TEST(Extract, TopBit) {
    EXPECT_EQ(extract(0x80000000, 31, 1), 0x1u);
}

TEST(SetBits, SetMiddleBits) {
    // 100100100 -> 100111100
    uint32_t mask = make_mask(3, 3);
    EXPECT_EQ(set_bits(0x124, mask), 0x13Cu);
}

TEST(SetBits, AlreadySetBitsAreUnchanged) {
    EXPECT_EQ(set_bits(0xFF, 0x0F), 0xFFu);
}

TEST(SetBits, EmptyMaskIsNoOp) {
    EXPECT_EQ(set_bits(0x55, 0), 0x55u);
}

TEST(SetBits, FullMaskSetsEverything) {
    EXPECT_EQ(set_bits(0, 0xFFFFFFFF), 0xFFFFFFFFu);
}

TEST(ClearBits, ClearMiddleBits) {
    // 11100110 -> 10000110 (clears bits 6:3)
    uint32_t mask = make_mask(3, 4);
    EXPECT_EQ(clear_bits(0xE6, mask), 0x86u);
}

TEST(ClearBits, AlreadyClearBitsAreUnchanged) {
    uint32_t mask = make_mask(3, 4);
    EXPECT_EQ(clear_bits(0x87, mask), 0x87u);
}

TEST(ClearBits, EmptyMaskIsNoOp) {
    EXPECT_EQ(clear_bits(0x55, make_mask(5, 0)), 0x55u);
}

TEST(ClearBits, FullMaskClearsEverything) {
    EXPECT_EQ(clear_bits(0xFFFFFFFF, make_mask(0, 32)), 0u);
}

TEST(TestBits, AllBitsSet) {
    // 11100110
    uint32_t mask = make_mask(6, 2);
    EXPECT_EQ(test_bits(0xE6, mask), true);
}

TEST(TestBits, NotAllBitsSet) {
    uint32_t mask = make_mask(0, 2);
    EXPECT_EQ(test_bits(0xE6, mask), false);
}

TEST(TestBits, PartialOverlapIsFalse) {
    uint32_t mask = make_mask(0, 8);
    EXPECT_EQ(test_bits(0x0F, mask), false);
}

TEST(TestBits, EmptyMaskIsAlwaysTrue) {
    EXPECT_EQ(test_bits(0x55, make_mask(5, 0)), true);
}

TEST(Insert, WritesFieldIntoEmptyRegister) {
    // Pin 5's MODER field (bits 11:10) set to mode 1 -> 0x400
    EXPECT_EQ(insert(0, 1, 10, 2), 0x400u);
}

TEST(Insert, OverwritesPreviousValueInField) {
    // Write 0b11 into a 2-bit field, then 0b01 -> must read back 0b01.
    // Catches a forgotten clear-before-OR step.
    uint32_t reg = insert(0, 0b11, 0, 2);
    reg = insert(reg, 0b01, 0, 2);
    EXPECT_EQ(reg, 0b01u);
}

TEST(Insert, DoesNotDisturbNeighboringBits) {
    EXPECT_EQ(insert(0xFFFFFFFF, 0b10, 4, 2), 0xFFFFFFEFu);
}

TEST(Insert, TruncatesValueWiderThanField) {
    // value = 0b101 (3 bits) into a 2-bit field: the extra high bit must be discarded, not spill into the next field.
    EXPECT_EQ(insert(0, 0b101, 0, 2), 0b01u);
}
