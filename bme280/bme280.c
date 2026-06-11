#include "bme280.h"

#include <furi.h>
#include <furi_hal_i2c.h>

#define TAG "BME280"

#define OSRS_T 1
#define OSRS_P 1
#define MODE   1

#define OSRS_H 1

const uint8_t bme_addr = 0xEC;

typedef struct {
    uint16_t T1;
    int16_t T2, T3;

    uint16_t P1;
    int16_t P2, P3, P4, P5, P6, P7, P8, P9;

    uint8_t H1, H3, H6;
    int16_t H2, H4, H5;

    uint8_t temp_msb, temp_lsb, temp_xlsb;
    uint8_t press_msb, press_lsb, press_xlsb;

    int32_t adc_T;
    int32_t adc_P;
    uint16_t adc_H;

    int32_t t_fine;

    uint8_t ctrl_meas;
} BME280;

static BME280 bme;

/* ---------------- I2C helpers ---------------- */

static bool read_u16(uint8_t reg, uint16_t* value) {
    uint8_t data[2];
    if(!furi_hal_i2c_trx(&furi_hal_i2c_handle_external, bme_addr, &reg, 1, data, 2, 100))
        return false;

    *value = (data[1] << 8) | data[0];
    return true;
}

static bool read_s16(uint8_t reg, int16_t* value) {
    uint8_t data[2];
    if(!furi_hal_i2c_trx(&furi_hal_i2c_handle_external, bme_addr, &reg, 1, data, 2, 100))
        return false;

    *value = (int16_t)((data[1] << 8) | data[0]);
    return true;
}

static bool read_u8(uint8_t reg, uint8_t* value) {
    return furi_hal_i2c_trx(&furi_hal_i2c_handle_external, bme_addr, &reg, 1, value, 1, 100);
}

/* ---------------- calibration ---------------- */

static bool read_calibration() {
    uint8_t e4, e5, e6;

    if(!read_u8(0xE4, &e4)) return false;
    if(!read_u8(0xE5, &e5)) return false;
    if(!read_u8(0xE6, &e6)) return false;

    bme.H4 = (int16_t)((e4 << 4) | (e5 & 0x0F));
    bme.H5 = (int16_t)((e6 << 4) | (e5 >> 4));

    return read_u16(0x88, &bme.T1) && read_s16(0x8A, &bme.T2) && read_s16(0x8C, &bme.T3) &&

           read_u16(0x8E, &bme.P1) && read_s16(0x90, &bme.P2) && read_s16(0x92, &bme.P3) &&
           read_s16(0x94, &bme.P4) && read_s16(0x96, &bme.P5) && read_s16(0x98, &bme.P6) &&
           read_s16(0x9A, &bme.P7) && read_s16(0x9C, &bme.P8) && read_s16(0x9E, &bme.P9) &&

           read_u8(0xA1, &bme.H1) && read_s16(0xE1, &bme.H2) && read_u8(0xE3, &bme.H3);
}

static bool configure_humidity() {
    uint8_t cmd[2] = {0xF2, OSRS_H};

    return furi_hal_i2c_tx(&furi_hal_i2c_handle_external, bme_addr, cmd, 2, 100);
}

/* ---------------- forced measurement ---------------- */

static bool trigger_measurement() {
    if(!configure_humidity()) return false;

    bme.ctrl_meas = (OSRS_T << 5) | (OSRS_P << 2) | MODE;

    uint8_t cmd[2] = {0xF4, bme.ctrl_meas};

    return furi_hal_i2c_tx(&furi_hal_i2c_handle_external, bme_addr, cmd, 2, 100);
}

/* ---------------- ADC reads ---------------- */

static bool read_adc_T() {
    uint8_t msb, lsb, xlsb;

    if(!read_u8(0xFA, &msb) || !read_u8(0xFB, &lsb) || !read_u8(0xFC, &xlsb)) return false;

    bme.adc_T = (msb << 12) | (lsb << 4) | (xlsb >> 4);
    return true;
}

static bool read_adc_P() {
    uint8_t msb, lsb, xlsb;

    if(!read_u8(0xF7, &msb) || !read_u8(0xF8, &lsb) || !read_u8(0xF9, &xlsb)) return false;

    bme.adc_P = (msb << 12) | (lsb << 4) | (xlsb >> 4);
    return true;
}

static bool read_adc_H() {
    uint8_t msb, lsb;

    if(!read_u8(0xFD, &msb) || !read_u8(0xFE, &lsb)) return false;

    bme.adc_H = ((uint16_t)msb << 8) | lsb;
    return true;
}

/* ---------------- temperature ---------------- */

static void calc_t_fine() {
    int32_t var1 = (((bme.adc_T >> 3) - (bme.T1 << 1)) * bme.T2) >> 11;
    int32_t var2 =
        (((((bme.adc_T >> 4) - bme.T1) * ((bme.adc_T >> 4) - bme.T1)) >> 12) * bme.T3) >> 14;

    bme.t_fine = var1 + var2;
}

/* ---------------- init ---------------- */

bool bme280_init(void) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    bool ok = read_calibration() && configure_humidity() && trigger_measurement();

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return ok;
}

/* ---------------- unified measurement ---------------- */

bool bme280_read(int32_t* temp, int32_t* press, int* rh) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(!trigger_measurement()) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        return false;
    }

    furi_delay_ms(10);

    if(!read_adc_T() || !read_adc_P() || !read_adc_H()) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        return false;
    }

    calc_t_fine();

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    /* temperature */
    *temp = ((bme.t_fine * 5 + 128) >> 8) / 10;

    /* pressure (full formula) */
    int64_t var1, var2, p;

    var1 = ((int64_t)bme.t_fine) - 128000;

    var2 = var1 * var1 * (int64_t)bme.P6;
    var2 += (var1 * (int64_t)bme.P5) << 17;
    var2 += ((int64_t)bme.P4) << 35;

    var1 = ((var1 * var1 * (int64_t)bme.P3) >> 8) + ((var1 * (int64_t)bme.P2) << 12);

    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)bme.P1) >> 33;

    if(var1 == 0) return false;

    p = 1048576 - bme.adc_P;
    p = ((p << 31) - var2) * 3125 / var1;

    var1 = ((int64_t)bme.P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)bme.P8 * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)bme.P7) << 4);

    *press = (int32_t)p / 256;

    /* humidity */
    double var_H;

    var_H = ((double)bme.t_fine) - 76800;
    var_H =
        (bme.adc_H - (((double)bme.H4) * 64 + ((double)bme.H5) / 16384 * var_H)) *
        (((double)bme.H2) / 65536 *
         (1 + ((double)bme.H6) / 67108864 * var_H * (1 + ((double)bme.H3) / 67108864 * var_H)));
    var_H = var_H * (1 - ((double)bme.H1) * var_H / 52488);

    if(var_H > 100)
        var_H = 100.0;
    else if(var_H < 0)
        var_H = 0.0;

    *rh = (int)var_H;

    return true;
}
