#include <cstring>
#include <gtest/gtest.h>
#include "../drivers/include/gpio.h"

/* The driver takes a pointer to a register struct rather than hard-coding a
   base address, so a plain struct in RAM stands in for the peripheral. Every
   test starts from an all-zero port and checks two things: the intended field
   changed, and nothing else did. */
static GPIO_Regs blank_port(void) {
    GPIO_Regs p;
    std::memset(&p, 0, sizeof(p));
    return p;
}

TEST(GpioSetMode, OutputOnPin5) {
    GPIO_Regs p = blank_port();
    gpio_set_mode(&p, 5, GPIO_MODE_OUTPUT);
    // pin 5 occupies MODER bits 11:10, value 01
    EXPECT_EQ(p.MODER, 1U << 10);
}

TEST(GpioSetMode, AnalogOnPin5) {
    GPIO_Regs p = blank_port();
    gpio_set_mode(&p, 5, GPIO_MODE_ANALOG);
    EXPECT_EQ(p.MODER, 3U << 10);
}

TEST(GpioSetMode, LeavesNeighbouringPinsAlone) {
    GPIO_Regs p = blank_port();
    gpio_set_mode(&p, 4, GPIO_MODE_ANALOG);
    gpio_set_mode(&p, 6, GPIO_MODE_ANALOG);
    gpio_set_mode(&p, 5, GPIO_MODE_OUTPUT);
    EXPECT_EQ(p.MODER, (3U << 8) | (1U << 10) | (3U << 12));
}

TEST(GpioSetMode, OverwritesRatherThanOrsIn) {
    // Catches forgotten clear-before-write: 11 then 01 must read back 01, not 11.
    GPIO_Regs p = blank_port();
    gpio_set_mode(&p, 5, GPIO_MODE_ANALOG);
    gpio_set_mode(&p, 5, GPIO_MODE_OUTPUT);
    EXPECT_EQ(p.MODER, 1U << 10);
}

TEST(GpioSetMode, TouchesOnlyMODER) {
    GPIO_Regs p = blank_port();
    gpio_set_mode(&p, 5, GPIO_MODE_OUTPUT);
    EXPECT_EQ(p.OTYPER, 0U);
    EXPECT_EQ(p.OSPEEDR, 0U);
    EXPECT_EQ(p.PUPDR, 0U);
    EXPECT_EQ(p.ODR, 0U);
    EXPECT_EQ(p.BSRR, 0U);
}

TEST(GpioSetPull, PullUpOnPin13) {
    GPIO_Regs p = blank_port();
    gpio_set_pull(&p, 13, GPIO_PUPD_PU);
    // pin 13 occupies PUPDR bits 27:26
    EXPECT_EQ(p.PUPDR, 1U << 26);
}

TEST(GpioSetPull, PullDownOnPin0) {
    GPIO_Regs p = blank_port();
    gpio_set_pull(&p, 0, GPIO_PUPD_PD);
    EXPECT_EQ(p.PUPDR, 2U);
}

TEST(GpioSetOtype, OpenDrainIsOneBitPerPin) {
    // OTYPER is 1 bit per pin, not 2. A pin*2 slip would land on bit 10.
    GPIO_Regs p = blank_port();
    gpio_set_otype(&p, 5, GPIO_TYPE_OPEN_DRAIN);
    EXPECT_EQ(p.OTYPER, 1U << 5);
}

TEST(GpioSetOspeed, HighSpeedOnPin5) {
    GPIO_Regs p = blank_port();
    gpio_set_ospeed(&p, 5, GPIO_SPEED_HIGH);
    EXPECT_EQ(p.OSPEEDR, 2U << 10);
}

TEST(GpioSetAf, Pin7GoesToLowRegister) {
    // Pin 7 is the last pin in AFR[0], at bits 31:28.
    GPIO_Regs p = blank_port();
    gpio_set_af(&p, 7, GPIO_AFR_AF3);
    EXPECT_EQ(p.AFR[0], 3U << 28);
    EXPECT_EQ(p.AFR[1], 0U);
}

TEST(GpioSetAf, Pin8GoesToHighRegister) {
    // Pin 8 is the first pin in AFR[1], at bits 3:0. Together with the test
    // above this pins down the AFR[pin / 8] boundary.
    GPIO_Regs p = blank_port();
    gpio_set_af(&p, 8, GPIO_AFR_AF5);
    EXPECT_EQ(p.AFR[0], 0U);
    EXPECT_EQ(p.AFR[1], 5U);
}

TEST(GpioSetAf, Pin12UsesFourBitField) {
    GPIO_Regs p = blank_port();
    gpio_set_af(&p, 12, GPIO_AFR_AF10);
    EXPECT_EQ(p.AFR[0], 0U);
    EXPECT_EQ(p.AFR[1], 0xAU << 16);
}

TEST(GpioWrite, SetUsesLowerHalfOfBSRR) {
    // BSRR bits 15:0 set the matching ODR bit.
    GPIO_Regs p = blank_port();
    gpio_write(&p, 5, 1);
    EXPECT_EQ(p.BSRR, 1U << 5);
}

TEST(GpioWrite, ClearUsesUpperHalfOfBSRR) {
    // BSRR bits 31:16 reset it. Swapping the two halves is the easy mistake.
    GPIO_Regs p = blank_port();
    gpio_write(&p, 5, 0);
    EXPECT_EQ(p.BSRR, 1U << 21);
}

TEST(GpioWrite, DoesNotTouchODRDirectly) {
    GPIO_Regs p = blank_port();
    gpio_write(&p, 5, 1);
    EXPECT_EQ(p.ODR, 0U);
}

TEST(GpioRead, ReturnsOneWhenPinHigh) {
    GPIO_Regs p = blank_port();
    p.IDR = 1U << 13;
    EXPECT_EQ(gpio_read(&p, 13), 1U);
}

TEST(GpioRead, ReturnsZeroWhenPinLow) {
    GPIO_Regs p = blank_port();
    p.IDR = 0U;
    EXPECT_EQ(gpio_read(&p, 13), 0U);
}

TEST(GpioRead, IgnoresNeighbouringPins) {
    // Every bit set except 13. A missing mask would return non-zero here.
    GPIO_Regs p = blank_port();
    p.IDR = ~(1U << 13);
    EXPECT_EQ(gpio_read(&p, 13), 0U);
}

TEST(GpioRead, NormalisesToZeroOrOne) {
    // Should return 1, not the raw masked word (0x2000).
    GPIO_Regs p = blank_port();
    p.IDR = 0xFFFFFFFFU;
    EXPECT_EQ(gpio_read(&p, 13), 1U);
}

TEST(GpioDriver, WorksOnAnyPortNotJustGPIOA) {
    // The whole point of passing a pointer: the same functions drive a
    // different port with no change. On hardware this is GPIOC for the
    // button and GPIOA for the LED.
    GPIO_Regs a = blank_port();
    GPIO_Regs c = blank_port();
    gpio_set_mode(&a, 5, GPIO_MODE_OUTPUT);
    gpio_set_mode(&c, 13, GPIO_MODE_INPUT);
    EXPECT_EQ(a.MODER, 1U << 10);
    EXPECT_EQ(c.MODER, 0U);
}
