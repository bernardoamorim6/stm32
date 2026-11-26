
#ifndef BMP280_H
#define BMP280_H

#include "i2c.h"
#include "stm32c031_regs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Factory trimming values from 0x88..0x9F, unique to each chip (datasheet
   Table 17). Only dig_T1 and dig_P1 are unsigned. */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} bmp280_calib_t;

/* Bus side. Each returns the state its I2C transfer ended in, so anything
   other than I2C_DONE means the outputs were not written. */
i2c_state_t bmp280_read_id(I2C_Regs *i2c, uint8_t *id);   /* 0x58 on a BMP280, 0x60 on a BME280 */
i2c_state_t bmp280_read_calib(I2C_Regs *i2c, bmp280_calib_t *cal);
i2c_state_t bmp280_start_forced(I2C_Regs *i2c);           /* one measurement, then back to sleep */
i2c_state_t bmp280_read_raw(I2C_Regs *i2c, int32_t *adc_T, int32_t *adc_P);

/* One forced measurement, compensated: temp in 0.01 degC, pascal in Pa. The
   calibration is read once by the caller; the outputs are only written on DONE. */
i2c_state_t bmp280_measurement(I2C_Regs *i2c, const bmp280_calib_t *cal, int32_t *temp, uint32_t *pascal);

/* Maths side: no hardware, so all of it is tested on the host. */
void bmp280_parse_calib(const uint8_t bytes[24], bmp280_calib_t *cal);

/* Temperature in 0.01 degC, so 2143 is 21.43 degC. t_fine is the input the
   pressure calculation needs, handed back here instead of kept in a global. */
int32_t bmp280_compensate_T(const bmp280_calib_t *cal, int32_t adc_T, int32_t *t_fine);

/* Pressure in Pa, from the t_fine of the same measurement. 0 means no result. */
uint32_t bmp280_compensate_P(const bmp280_calib_t *cal, int32_t adc_P, int32_t t_fine);


#ifdef __cplusplus
}
#endif

#endif // BMP280_H
