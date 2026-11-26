#include <cstdint>
#include <gtest/gtest.h>
#include "../drivers/include/formatter.h"

/* Pure maths, no hardware. Every test starts from a buffer full of 1s, so a
   missing '\0' cannot hide behind a leftover zero. */
static void fill_array(char *s){
    for(int i = 0; i < FORMATTED_MEASUREMENT_SIZE; i++){
        s[i] = 1;
    }
}

TEST(MeasurementFormatter, PositiveGetsTwoDecimals) {
    char s[FORMATTED_MEASUREMENT_SIZE];
    fill_array(s);
    int32_t value = 2143;
    measurement_formatter(value, s);
    EXPECT_EQ(s[0], '2');
    EXPECT_EQ(s[1], '1');
    EXPECT_EQ(s[2], '.');
    EXPECT_EQ(s[3], '4');
    EXPECT_EQ(s[4], '3');
    EXPECT_EQ(s[5], '\0');
}

TEST(MeasurementFormatter, NegativeGetsAMinusSign) {
    char s[FORMATTED_MEASUREMENT_SIZE];
    fill_array(s);
    int32_t value = -2143;
    measurement_formatter(value, s);

    EXPECT_EQ(s[0], '-');
    EXPECT_EQ(s[1], '2');
    EXPECT_EQ(s[2], '1');
    EXPECT_EQ(s[3], '.');
    EXPECT_EQ(s[4], '4');
    EXPECT_EQ(s[5], '3');
    EXPECT_EQ(s[6], '\0');
}

TEST(MeasurementFormatter, NegativeUnderOneGetsALeadingZero) {
    char s[FORMATTED_MEASUREMENT_SIZE];
    fill_array(s);
    int32_t value = -21;
    measurement_formatter(value, s);

    EXPECT_EQ(s[0], '-');
    EXPECT_EQ(s[1], '0');
    EXPECT_EQ(s[2], '.');
    EXPECT_EQ(s[3], '2');
    EXPECT_EQ(s[4], '1');
    EXPECT_EQ(s[5], '\0');
}


TEST(MeasurementFormatter, ZeroIsZeroPointZeroZero) {
    char s[FORMATTED_MEASUREMENT_SIZE];
    fill_array(s);
    int32_t value = 0;
    measurement_formatter(value, s);

    EXPECT_EQ(s[0], '0');
    EXPECT_EQ(s[1], '.');
    EXPECT_EQ(s[2], '0');
    EXPECT_EQ(s[3], '0');
    EXPECT_EQ(s[4], '\0');
}

/* The longest possible text, so it also proves FORMATTED_MEASUREMENT_SIZE is enough */
TEST(MeasurementFormatter, Int32MinFillsTheWholeBuffer) {
    char s[FORMATTED_MEASUREMENT_SIZE];
    fill_array(s);
    int32_t value = INT32_MIN;
    measurement_formatter(value, s);
    EXPECT_EQ(s[0], '-');
    EXPECT_EQ(s[1], '2');
    EXPECT_EQ(s[2], '1');
    EXPECT_EQ(s[3], '4');
    EXPECT_EQ(s[4], '7');
    EXPECT_EQ(s[5], '4');
    EXPECT_EQ(s[6], '8');
    EXPECT_EQ(s[7], '3');
    EXPECT_EQ(s[8], '6');
    EXPECT_EQ(s[9], '.');
    EXPECT_EQ(s[10], '4');
    EXPECT_EQ(s[11], '8');
    EXPECT_EQ(s[12], '\0');
}

TEST(MeasurementFormatter, OnlyHundredthsKeepsBothZeros) {
    char s[FORMATTED_MEASUREMENT_SIZE];
    fill_array(s);
    int32_t value = -5;
    measurement_formatter(value, s);
    EXPECT_EQ(s[0], '-');
    EXPECT_EQ(s[1], '0');
    EXPECT_EQ(s[2], '.');
    EXPECT_EQ(s[3], '0');
    EXPECT_EQ(s[4], '5');
    EXPECT_EQ(s[5], '\0');
}
