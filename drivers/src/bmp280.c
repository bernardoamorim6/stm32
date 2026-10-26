#include "../include/bmp280.h"
#include <stdint.h>

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
