#include <cstring>
#include <gtest/gtest.h>
#include "../drivers/include/i2c.h"

/* Bit values written out from RM0490 rather than taken from i2c.h, so a wrong
   #define in the driver cannot be hidden by the same wrong value here. */
namespace {
constexpr uint32_t TXIS  = 1U << 1;
constexpr uint32_t RXNE  = 1U << 2;
constexpr uint32_t NACKF = 1U << 4;
constexpr uint32_t STOPF = 1U << 5;
constexpr uint32_t TC    = 1U << 6;
constexpr uint32_t BERR  = 1U << 8;
constexpr uint32_t ARLO  = 1U << 9;

constexpr uint32_t NACKCF = 1U << 4;
constexpr uint32_t STOPCF = 1U << 5;
constexpr uint32_t BERRCF = 1U << 8;
constexpr uint32_t ARLOCF = 1U << 9;

/* Address 0x76, worked out by hand:
   write phase: SADD 0x76 << 1 = 0xEC, NBYTES 1 << 16, START 1 << 13           */
constexpr uint32_t CR2_WRITE_REG = 0x000120ECU;
/* read phase: the above plus RD_WRN 1 << 10 and AUTOEND 1 << 25, NBYTES = n   */
constexpr uint32_t CR2_READ_1    = 0x020124ECU;
constexpr uint32_t CR2_READ_3    = 0x020324ECU;
}  // namespace

/* The fake has no side effects: writing TXDR does not clear TXIS, writing ICR
   does not clear anything. So each test plays the hardware, setting ISR to
   whatever the peripheral would be reporting before every step. */
class I2cStep : public ::testing::Test {
protected:
    I2C_Regs fake{};
    uint8_t buf[8]{};
    i2c_xfer_t x{};

    void SetUp() override {
        std::memset(&fake, 0, sizeof(fake));
        std::memset(buf, 0, sizeof(buf));
    }

    void begin(uint8_t len, uint32_t now = 0, uint32_t timeout = 10) {
        i2c_xfer_begin(&x, 0x76, 0xD0, buf, len, now, timeout);
    }

    i2c_state_t step(uint32_t isr, uint32_t now = 0) {
        fake.ISR = isr;
        return i2c_step(&fake, &x, now);
    }

    // Drive a fresh one-byte read to a given wait state, playing the hardware.
    void to_wait_txis()  { begin(1); step(0); }
    void to_wait_tc()    { to_wait_txis(); step(TXIS); }
    void to_wait_rxne()  { to_wait_tc(); step(TC); }
    void to_wait_stopf() { to_wait_rxne(); fake.RXDR = 0x60; step(RXNE); }
};

/* --- init ----------------------------------------------------------------- */

TEST(I2cInit, TimingFromTable114AndEnabled) {
    I2C_Regs fake{};
    i2c_init(&fake);
    EXPECT_EQ(fake.TIMINGR, 0x30420F13U);   // 16 MHz, 100 kHz
    EXPECT_EQ(fake.CR1, 0x1U);              // PE only
}

TEST_F(I2cStep, BeginTouchesNoHardware) {
    begin(1);
    EXPECT_EQ(x.state, I2C_START_WRITE);
    EXPECT_EQ(x.count, 0);
    EXPECT_EQ(fake.CR2, 0U);
}

/* --- the happy path, one arrow at a time ----------------------------------- */

TEST_F(I2cStep, StartSendsAddressForWriteWithoutAutoend) {
    begin(1);
    EXPECT_EQ(step(0), I2C_WAIT_TXIS);
    EXPECT_EQ(fake.CR2, CR2_WRITE_REG);
}

TEST_F(I2cStep, WaitingForTxisDoesNothingUntilItArrives) {
    to_wait_txis();
    EXPECT_EQ(step(0), I2C_WAIT_TXIS);
    EXPECT_EQ(fake.TXDR, 0U);
}

TEST_F(I2cStep, TxisSendsTheRegisterNumber) {
    to_wait_txis();
    EXPECT_EQ(step(TXIS), I2C_WAIT_TC);
    EXPECT_EQ(fake.TXDR, 0xD0U);
}

TEST_F(I2cStep, TcIssuesRepeatedStartForRead) {
    to_wait_tc();
    EXPECT_EQ(step(TC), I2C_WAIT_RXNE);
    EXPECT_EQ(fake.CR2, CR2_READ_1);
}

TEST_F(I2cStep, ReadPhaseAsksForTheRequestedLength) {
    begin(3);
    step(0);
    step(TXIS);
    step(TC);
    EXPECT_EQ(fake.CR2, CR2_READ_3);
}

TEST_F(I2cStep, RxneStoresTheByte) {
    to_wait_rxne();
    fake.RXDR = 0x60;
    EXPECT_EQ(step(RXNE), I2C_WAIT_STOPF);
    EXPECT_EQ(buf[0], 0x60);
}

TEST_F(I2cStep, BurstReadKeepsOrderAndWaitsForEveryByte) {
    begin(3);
    step(0);
    step(TXIS);
    step(TC);
    fake.RXDR = 0x11;
    EXPECT_EQ(step(RXNE), I2C_WAIT_RXNE);
    fake.RXDR = 0x22;
    EXPECT_EQ(step(RXNE), I2C_WAIT_RXNE);
    fake.RXDR = 0x33;
    EXPECT_EQ(step(RXNE), I2C_WAIT_STOPF);
    EXPECT_EQ(buf[0], 0x11);
    EXPECT_EQ(buf[1], 0x22);
    EXPECT_EQ(buf[2], 0x33);
}

TEST_F(I2cStep, StopfFinishesAndClearsIt) {
    to_wait_stopf();
    EXPECT_EQ(step(STOPF), I2C_DONE);
    EXPECT_NE(fake.ICR & STOPCF, 0U);
}

TEST_F(I2cStep, WholeChipIdReadEndToEnd) {
    begin(1);
    EXPECT_EQ(step(0), I2C_WAIT_TXIS);
    EXPECT_EQ(step(TXIS), I2C_WAIT_TC);
    EXPECT_EQ(step(TC), I2C_WAIT_RXNE);
    fake.RXDR = 0x60;                       // a genuine BME280
    EXPECT_EQ(step(RXNE), I2C_WAIT_STOPF);
    EXPECT_EQ(step(STOPF), I2C_DONE);
    EXPECT_EQ(buf[0], 0x60);
}

/* --- NACKs: wait for the automatic STOP, then report which one ------------- */

TEST_F(I2cStep, NackOnAddressWaitsForTheStop) {
    to_wait_txis();
    EXPECT_EQ(step(NACKF), I2C_WAIT_STOPF);
    EXPECT_EQ(fake.ICR, 0U) << "nothing should be cleared before the STOP completes";
}

TEST_F(I2cStep, NackStillSetWhileWaitingForStopIsNotANewNack) {
    to_wait_txis();
    step(NACKF);
    EXPECT_EQ(step(NACKF), I2C_WAIT_STOPF);  // NACKF stays set until we clear it
    EXPECT_EQ(fake.ICR, 0U);
}

TEST_F(I2cStep, NackOnAddressEndsAsAddressErrorAndClearsBoth) {
    to_wait_txis();
    step(NACKF);
    EXPECT_EQ(step(NACKF | STOPF), I2C_ERR_NACK_ADDR);
    EXPECT_NE(fake.ICR & NACKCF, 0U);
    EXPECT_NE(fake.ICR & STOPCF, 0U);
}

TEST_F(I2cStep, NackOnRegisterByteEndsAsDataError) {
    to_wait_tc();
    EXPECT_EQ(step(NACKF), I2C_WAIT_STOPF);
    EXPECT_EQ(step(NACKF | STOPF), I2C_ERR_NACK_DATA);
    EXPECT_NE(fake.ICR & NACKCF, 0U);
    EXPECT_NE(fake.ICR & STOPCF, 0U);
}

TEST_F(I2cStep, NackOnReadAddressEndsAsAddressError) {
    to_wait_rxne();
    EXPECT_EQ(step(NACKF), I2C_WAIT_STOPF);
    EXPECT_EQ(step(NACKF | STOPF), I2C_ERR_NACK_ADDR);
}

/* --- bus errors ------------------------------------------------------------ */

TEST_F(I2cStep, BusErrorEndsTheTransferAndClears) {
    to_wait_txis();
    EXPECT_EQ(step(BERR), I2C_ERR_BUS);
    EXPECT_NE(fake.ICR & BERRCF, 0U);
}

TEST_F(I2cStep, ArbitrationLossEndsTheTransferAndClears) {
    to_wait_tc();
    EXPECT_EQ(step(ARLO), I2C_ERR_BUS);
    EXPECT_NE(fake.ICR & ARLOCF, 0U);
}

TEST_F(I2cStep, BusErrorWinsOverTheFlagBeingWaitedFor) {
    to_wait_txis();
    EXPECT_EQ(step(TXIS | BERR), I2C_ERR_BUS);
    EXPECT_EQ(fake.TXDR, 0U) << "no byte should go out on a broken bus";
}

/* --- timeouts: begin at t = 0 with a 10 ms budget -------------------------- */

TEST_F(I2cStep, TimesOutOnceTheBudgetIsExceeded) {
    to_wait_txis();
    EXPECT_EQ(step(0, 11), I2C_ERR_TIMEOUT);
}

TEST_F(I2cStep, ExactlyAtTheBudgetIsNotYetATimeout) {
    to_wait_txis();
    EXPECT_EQ(step(0, 10), I2C_WAIT_TXIS);
}

TEST_F(I2cStep, EveryWaitStateCanTimeOut) {
    to_wait_tc();
    EXPECT_EQ(step(0, 11), I2C_ERR_TIMEOUT) << "WAIT_TC";
    to_wait_rxne();
    EXPECT_EQ(step(0, 11), I2C_ERR_TIMEOUT) << "WAIT_RXNE";
    to_wait_stopf();
    EXPECT_EQ(step(0, 11), I2C_ERR_TIMEOUT) << "WAIT_STOPF";
}

TEST_F(I2cStep, AFlagThatHasArrivedBeatsTheTimeout) {
    to_wait_txis();
    EXPECT_EQ(step(TXIS, 50), I2C_WAIT_TC);
}

TEST_F(I2cStep, TimeoutSurvivesTheTickCounterWrapping) {
    begin(1, 0xFFFFFFF0U);                  // 16 ms before the 32-bit wrap
    step(0, 0xFFFFFFF0U);
    EXPECT_EQ(step(0, 0xFFFFFFF9U), I2C_WAIT_TXIS) << "9 ms elapsed";
    EXPECT_EQ(step(0, 0x00000004U), I2C_ERR_TIMEOUT) << "20 ms elapsed, across the wrap";
}

/* --- terminal states are inert --------------------------------------------- */

TEST_F(I2cStep, TerminalStatesWriteNothing) {
    const i2c_state_t terminal[] = {I2C_DONE, I2C_ERR_NACK_ADDR, I2C_ERR_NACK_DATA,
                                    I2C_ERR_BUS, I2C_ERR_TIMEOUT};
    for (i2c_state_t s : terminal) {
        std::memset(&fake, 0, sizeof(fake));
        begin(1);
        x.state = s;
        EXPECT_EQ(step(TXIS | RXNE | NACKF | STOPF | TC | BERR | ARLO, 100), s);
        EXPECT_EQ(fake.CR2, 0U);
        EXPECT_EQ(fake.TXDR, 0U);
        EXPECT_EQ(fake.ICR, 0U);
    }
}
