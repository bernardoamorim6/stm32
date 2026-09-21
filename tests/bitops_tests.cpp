#include <gtest/gtest.h>
#include "../drivers/include/bitops.h"

// Demonstrate some basic assertions.
TEST(CreateMask, TwoBitFieldAtTen) {
    EXPECT_EQ(make_mask(10, 2), 0xC00u);
}

TEST(CreateMask, SingleBitAtZero) {
    EXPECT_EQ(make_mask(0, 1), 1);
}

TEST(CreateMask, TopBit) {
    EXPECT_EQ(make_mask(31, 1), 0x80000000);
}

TEST(CreateMask, FullWidth) {
    EXPECT_EQ(make_mask(0, 32), 0xFFFFFFFF);
}


