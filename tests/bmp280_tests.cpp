#include <gtest/gtest.h>
#include "../drivers/include/bmp280.h"

/* Pure maths: numbers in, numbers out, no fake registers. Expected integer
   results come from compiling the datasheet's appendix 8.2 code unchanged. */
namespace {

/* Datasheet section 3.12, the worked example on page 23 */
constexpr bmp280_calib_t kExample = {27504, 26435, -1000, 36477, -10685, 3024,
                                     2855, 140, -7, 15500, -14600, 6000};
constexpr int32_t kExampleAdcT = 519888;
constexpr int32_t kExampleAdcP = 415148;

/* My board: the 24 bytes read from 0x88..0x9F, and one forced measurement */
constexpr uint8_t kMyCalibBytes[24] = {0xD2, 0x6D, 0x8E, 0x66, 0x32, 0x00, 0x8C, 0x90,
                                       0x5E, 0xD6, 0xD0, 0x0B, 0xD2, 0x1A, 0x3B, 0xFF,
                                       0xF9, 0xFF, 0x8C, 0x3C, 0xF8, 0xC6, 0x70, 0x17};
constexpr int32_t kMyAdcT = 518304;
constexpr int32_t kMyAdcP = 330032;

bmp280_calib_t my_calib() {
    bmp280_calib_t cal{};
    bmp280_parse_calib(kMyCalibBytes, &cal);
    return cal;
}

/* The double precision formula from appendix 8.1, an independent reference:
   a different calculation from 8.2, so agreeing with it means something. */
struct Reference {
    double celsius;
    double pascal;
};

Reference reference(const bmp280_calib_t &c, int32_t adc_T, int32_t adc_P) {
    double var1 = ((adc_T / 16384.0) - (c.dig_T1 / 1024.0)) * c.dig_T2;
    double var2 = ((adc_T / 131072.0) - (c.dig_T1 / 8192.0))
                * ((adc_T / 131072.0) - (c.dig_T1 / 8192.0)) * c.dig_T3;
    const double t_fine = var1 + var2;
    const double celsius = t_fine / 5120.0;

    var1 = (t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * c.dig_P6 / 32768.0;
    var2 = var2 + (var1 * c.dig_P5 * 2.0);
    var2 = (var2 / 4.0) + (c.dig_P4 * 65536.0);
    var1 = ((c.dig_P3 * var1 * var1 / 524288.0) + (c.dig_P2 * var1)) / 524288.0;
    var1 = (1.0 + (var1 / 32768.0)) * c.dig_P1;
    double p = 1048576.0 - adc_P;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = c.dig_P9 * p * p / 2147483648.0;
    var2 = p * c.dig_P8 / 32768.0;
    p = p + ((var1 + var2 + c.dig_P7) / 16.0);
    return {celsius, p};
}

/* 8.2 claims about 1 Pa (1 sigma); the datasheet's own example is 2.7 Pa apart */
constexpr double kTempTolerance     = 1.0;   /* in 0.01 degC */
constexpr double kPressureTolerance = 4.0;   /* in Pa */

}  // namespace

/* --- calibration bytes to values ------------------------------------------- */

TEST(Bmp280ParseCalib, MyChipsBytesGiveAllTwelveValues) {
    const bmp280_calib_t cal = my_calib();
    EXPECT_EQ(cal.dig_T1, 28114);
    EXPECT_EQ(cal.dig_T2, 26254);
    EXPECT_EQ(cal.dig_T3, 50);
    EXPECT_EQ(cal.dig_P1, 37004);
    EXPECT_EQ(cal.dig_P2, -10658);
    EXPECT_EQ(cal.dig_P3, 3024);
    EXPECT_EQ(cal.dig_P4, 6866);
    EXPECT_EQ(cal.dig_P5, -197);
    EXPECT_EQ(cal.dig_P6, -7);
    EXPECT_EQ(cal.dig_P7, 15500);
    EXPECT_EQ(cal.dig_P8, -14600);
    EXPECT_EQ(cal.dig_P9, 6000);
}

TEST(Bmp280ParseCalib, UnsignedValuesKeepTheirTopBit) {
    uint8_t bytes[24] = {};
    bytes[0] = 0xFF;   /* dig_T1 = 0xFFFF */
    bytes[1] = 0xFF;
    bytes[6] = 0x00;   /* dig_P1 = 0x8000 */
    bytes[7] = 0x80;
    bmp280_calib_t cal{};
    bmp280_parse_calib(bytes, &cal);
    EXPECT_EQ(cal.dig_T1, 65535) << "dig_T1 is unsigned, so 0xFFFF must not become -1";
    EXPECT_EQ(cal.dig_P1, 32768);
}

/* --- the datasheet's worked example ---------------------------------------- */

TEST(Bmp280Compensate, DatasheetExampleTemperature) {
    int32_t t_fine = 0;
    EXPECT_EQ(bmp280_compensate_T(&kExample, kExampleAdcT, &t_fine), 2508);   /* 25.08 degC */
    EXPECT_EQ(t_fine, 128422);
}

/* The figure prints 100653 as its integer result, but the appendix 8.2 code
   gives 100656. Footnote (**) in the figure warns about exactly this. */
TEST(Bmp280Compensate, DatasheetExamplePressure) {
    int32_t t_fine = 0;
    bmp280_compensate_T(&kExample, kExampleAdcT, &t_fine);
    EXPECT_EQ(bmp280_compensate_P(&kExample, kExampleAdcP, t_fine), 100656U);
}

/* --- my chip ---------------------------------------------------------------- */

TEST(Bmp280Compensate, MyReading) {
    const bmp280_calib_t cal = my_calib();
    int32_t t_fine = 0;
    EXPECT_EQ(bmp280_compensate_T(&cal, kMyAdcT, &t_fine), 2143);    /* 21.43 degC */
    EXPECT_EQ(t_fine, 109746);
    EXPECT_EQ(bmp280_compensate_P(&cal, kMyAdcP, t_fine), 102137U);  /* 1021.37 hPa */
}

/* --- against the floating point formula ------------------------------------ */

TEST(Bmp280Compensate, AgreesWithFloatingPointOnTheExample) {
    const Reference ref = reference(kExample, kExampleAdcT, kExampleAdcP);
    int32_t t_fine = 0;
    EXPECT_NEAR(bmp280_compensate_T(&kExample, kExampleAdcT, &t_fine), ref.celsius * 100.0,
                kTempTolerance);
    EXPECT_NEAR(bmp280_compensate_P(&kExample, kExampleAdcP, t_fine), ref.pascal,
                kPressureTolerance);
}

TEST(Bmp280Compensate, AgreesWithFloatingPointOnMyReading) {
    const bmp280_calib_t cal = my_calib();
    const Reference ref = reference(cal, kMyAdcT, kMyAdcP);
    int32_t t_fine = 0;
    EXPECT_NEAR(bmp280_compensate_T(&cal, kMyAdcT, &t_fine), ref.celsius * 100.0,
                kTempTolerance);
    EXPECT_NEAR(bmp280_compensate_P(&cal, kMyAdcP, t_fine), ref.pascal, kPressureTolerance);
}

/* A raw reading well below my room's: the signed arithmetic must survive a
   negative t_fine. About -15.6 degC with my calibration. */
TEST(Bmp280Compensate, BelowZeroAgreesWithFloatingPoint) {
    const bmp280_calib_t cal = my_calib();
    const int32_t cold_adc_T = 400000;
    const Reference ref = reference(cal, cold_adc_T, kMyAdcP);
    int32_t t_fine = 0;
    const int32_t celsius_x100 = bmp280_compensate_T(&cal, cold_adc_T, &t_fine);
    EXPECT_LT(t_fine, 0);
    EXPECT_LT(celsius_x100, 0);
    EXPECT_NEAR(celsius_x100, ref.celsius * 100.0, kTempTolerance);
    EXPECT_NEAR(bmp280_compensate_P(&cal, kMyAdcP, t_fine), ref.pascal, kPressureTolerance);
}

/* --- design properties -------------------------------------------------------- */

/* No hidden t_fine: pressure depends only on its arguments, so calling the
   temperature function for another reading in between changes nothing. */
TEST(Bmp280Compensate, PressureDependsOnlyOnItsArguments) {
    int32_t t_fine = 0;
    bmp280_compensate_T(&kExample, kExampleAdcT, &t_fine);
    int32_t other = 0;
    bmp280_compensate_T(&kExample, 400000, &other);
    EXPECT_EQ(bmp280_compensate_P(&kExample, kExampleAdcP, t_fine), 100656U);
}

TEST(Bmp280Compensate, ZeroDivisorReturnsZeroInsteadOfDividing) {
    bmp280_calib_t broken = kExample;
    broken.dig_P1 = 0;   /* makes the divisor var1 zero */
    int32_t t_fine = 0;
    bmp280_compensate_T(&broken, kExampleAdcT, &t_fine);
    EXPECT_EQ(bmp280_compensate_P(&broken, kExampleAdcP, t_fine), 0U);
}
