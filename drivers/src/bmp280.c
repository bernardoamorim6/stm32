#include "../include/bmp280.h"
#include "../include/systick.h"
#include <stdint.h>

#define BMP280_ADDR       0x76U
#define BMP280_REG_ID     0xD0U
#define BMP280_REG_CALIB  0x88U
#define BMP280_REG_CTRL   0xF4U
#define BMP280_REG_DATA   0xF7U
#define BMP280_CTRL_FORCED_X1  0x25U   /* osrs_t 001, osrs_p 001, mode 01 */

/* Each value is two bytes, low byte first. The cast to int16_t is what makes
   the top bit a sign for the ten signed values. */
void bmp280_parse_calib(const uint8_t bytes[24], bmp280_calib_t *cal){
    cal->dig_T1 = (uint16_t)(((uint16_t)bytes[1] << 8) | bytes[0]);
    cal->dig_T2 = (int16_t)(((uint16_t)bytes[3] << 8) | bytes[2]);
    cal->dig_T3 = (int16_t)(((uint16_t)bytes[5] << 8) | bytes[4]);
    cal->dig_P1 = (uint16_t)(((uint16_t)bytes[7] << 8) | bytes[6]);
    cal->dig_P2 = (int16_t)(((uint16_t)bytes[9] << 8) | bytes[8]);
    cal->dig_P3 = (int16_t)(((uint16_t)bytes[11] << 8) | bytes[10]);
    cal->dig_P4 = (int16_t)(((uint16_t)bytes[13] << 8) | bytes[12]);
    cal->dig_P5 = (int16_t)(((uint16_t)bytes[15] << 8) | bytes[14]);
    cal->dig_P6 = (int16_t)(((uint16_t)bytes[17] << 8) | bytes[16]);
    cal->dig_P7 = (int16_t)(((uint16_t)bytes[19] << 8) | bytes[18]);
    cal->dig_P8 = (int16_t)(((uint16_t)bytes[21] << 8) | bytes[20]);
    cal->dig_P9 = (int16_t)(((uint16_t)bytes[23] << 8) | bytes[22]);
}

/* Datasheet appendix 8.2, 32-bit fixed point: no FPU, and 64-bit multiplies
   are libgcc calls on the M0+. Relies on >> sign-extending, which GCC does. */
int32_t bmp280_compensate_T(const bmp280_calib_t *cal, int32_t adc_T, int32_t *t_fine){
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)cal->dig_T1<<1))) * ((int32_t)cal->dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)cal->dig_T1)) * ((adc_T>>4) - ((int32_t)cal->dig_T1))) >> 12) * ((int32_t)cal->dig_T3)) >> 14;
    int32_t T;
    *t_fine = var1 + var2;
    T = ((*t_fine * 5) + 128) >> 8;
    return T;
}

uint32_t bmp280_compensate_P(const bmp280_calib_t *cal, int32_t adc_P, int32_t t_fine){
    int32_t var1 = ((t_fine) >> 1) - (int32_t)64000;
    int32_t var2 = (((var1>>2) * (var1>>2)) >> 11) * ((int32_t)cal->dig_P6);
    var2 = var2 + ((var1 * ((int32_t)cal->dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)cal->dig_P4) << 16);
    var1 = (((cal->dig_P3 * (((var1 >> 2) * (var1>>2)) >> 13 )) >> 3) + ((((int32_t)cal->dig_P2) * var1)>>1))>>18;
    var1 = ((((32768 + var1)) * ((int32_t)cal->dig_P1)) >> 15);
    /* A corrupt calibration can make the divisor zero */
    if (var1 == 0){
        return 0;
    }
    uint32_t p = (((uint32_t)(((int32_t)1048576)-adc_P)-(var2>>12)))*3125;
    /* Shift before dividing keeps a bit of precision, but only while p << 1 fits */
    if (p < 0x80000000) {
        p = (p<<1) / ((uint32_t)var1);
    } else {
        p = (p / (uint32_t)var1) * 2;
    }
    var1 = (((int32_t)cal->dig_P9) * ((int32_t)(((p>>3) * (p>>3))>>13)))>>12;
    var2 = (((int32_t)(p>>2)) * ((int32_t)cal->dig_P8))>>13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + cal->dig_P7) >> 4));
    return p;
}

i2c_state_t bmp280_read_id(I2C_Regs *i2c, uint8_t *id){
    i2c_state_t st = i2c_read_reg(i2c, BMP280_ADDR, BMP280_REG_ID, id, sizeof(*id));
    return st;
}

/* 0x88..0x9F. The datasheet lists the block up to 0xA1, but 0xA0/0xA1 are reserved. */
i2c_state_t bmp280_read_calib(I2C_Regs *i2c, bmp280_calib_t *cal){
    uint8_t bytes[24] = {0};
    i2c_state_t st = i2c_read_reg(i2c, BMP280_ADDR, BMP280_REG_CALIB, bytes, sizeof(bytes));
    if (st == I2C_DONE) {
        bmp280_parse_calib(bytes, cal);
    }
    return st;
}

i2c_state_t bmp280_start_forced(I2C_Regs *i2c){
    i2c_state_t st = i2c_write_reg(i2c, BMP280_ADDR, BMP280_REG_CTRL, BMP280_CTRL_FORCED_X1);
    return st;
}

/* One 6-byte burst: the chip shadows the data registers during a burst, so a
   measurement finishing mid-read cannot mix bytes from two readings. */
i2c_state_t bmp280_read_raw(I2C_Regs *i2c, int32_t *adc_T, int32_t *adc_P){
    uint8_t data[6] = {0};
    i2c_state_t st = i2c_read_reg(i2c, BMP280_ADDR, BMP280_REG_DATA, data, sizeof(data));
    if (st == I2C_DONE) {
        /* msb holds bits 19:12, lsb 11:4, and xlsb keeps 3:0 in its top nibble */
        *adc_P = (int32_t)(((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | ((uint32_t)data[2] >> 4));
        *adc_T = (int32_t)(((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | ((uint32_t)data[5] >> 4));
    }
    return st;
}

/* No calibration read here: it is fixed at the factory, so the caller reads it
   once at boot instead of 24 bytes on every measurement */
i2c_state_t bmp280_measurement(I2C_Regs *i2c, const bmp280_calib_t *cal, int32_t *temp, uint32_t *pascal){

    int32_t adc_T = 0;
    int32_t adc_P = 0;
    int32_t t_fine = 0;

    i2c_state_t st = bmp280_start_forced(i2c);
    if (st != I2C_DONE) {
        return st;
    }
    delay_ms(10);   /* x1 oversampling takes 6.4 ms at most (datasheet Table 13) */
    st = bmp280_read_raw(i2c, &adc_T, &adc_P);
    if (st != I2C_DONE) {
        return st;
    }

    *temp = bmp280_compensate_T(cal, adc_T, &t_fine);
    *pascal = bmp280_compensate_P(cal, adc_P, t_fine);

    return st;
}
