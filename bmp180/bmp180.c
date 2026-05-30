#include "bmp180.h"

#include <furi.h>
#include <furi_hal_i2c.h>

const uint8_t bmp_addr = 0xEE;

typedef struct {
    int16_t AC1;
    int16_t AC2;
    int16_t AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;
    int16_t B1;
    int16_t B2;
    int16_t MC;
    int16_t MD;
} BMP180Calibration;

static BMP180Calibration calib;
static bool calib_loaded = false;

static bool bmp_read_u16(uint8_t reg, uint16_t* value);
static bool bmp_read_s16(uint8_t reg, int16_t* value);
static bool bmp_read_calibration(BMP180Calibration* c);
static bool bmp_read_raw_temperature(int32_t* ut);
static bool bmp_read_raw_pressure(int32_t* up);
static int32_t bmp_calculate_b5(int32_t ut, const BMP180Calibration* c);

bool bmp180_init(void) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    bool ok = bmp_read_calibration(&calib);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    calib_loaded = ok;

    return ok;
}

static bool bmp_read_u16(uint8_t reg, uint16_t* value) {
    uint8_t data[2];

    if(!furi_hal_i2c_trx(&furi_hal_i2c_handle_external, bmp_addr, &reg, 1, data, 2, 100)) {
        return false;
    }

    *value = ((uint16_t)data[0] << 8) | data[1];
    return true;
}

static bool bmp_read_s16(uint8_t reg, int16_t* value) {
    uint16_t temp;

    if(!bmp_read_u16(reg, &temp)) {
        return false;
    }

    *value = (int16_t)temp;
    return true;
}

static bool bmp_read_calibration(BMP180Calibration* c) {
    return bmp_read_s16(0xAA, &c->AC1) && bmp_read_s16(0xAC, &c->AC2) &&
           bmp_read_s16(0xAE, &c->AC3) && bmp_read_u16(0xB0, &c->AC4) &&
           bmp_read_u16(0xB2, &c->AC5) && bmp_read_u16(0xB4, &c->AC6) &&
           bmp_read_s16(0xB6, &c->B1) && bmp_read_s16(0xB8, &c->B2) &&
           bmp_read_s16(0xBC, &c->MC) && bmp_read_s16(0xBE, &c->MD);
}

static bool bmp_read_raw_temperature(int32_t* ut) {
    uint8_t cmd[2] = {0xF4, 0x2E};
    uint8_t reg = 0xF6;
    uint8_t data[2];

    furi_hal_i2c_tx(&furi_hal_i2c_handle_external, bmp_addr, cmd, 2, 100);

    furi_delay_ms(5);

    if(!furi_hal_i2c_trx(&furi_hal_i2c_handle_external, bmp_addr, &reg, 1, data, 2, 100)) {
        return false;
    }

    *ut = ((int32_t)data[0] << 8) | data[1];
    return true;
}

static bool bmp_read_raw_pressure(int32_t* up) {
    uint8_t cmd[2] = {0xF4, 0x34}; // OSS = 0
    uint8_t reg = 0xF6;
    uint8_t data[3];

    furi_hal_i2c_tx(&furi_hal_i2c_handle_external, bmp_addr, cmd, 2, 100);

    furi_delay_ms(5);

    if(!furi_hal_i2c_trx(&furi_hal_i2c_handle_external, bmp_addr, &reg, 1, data, 3, 100)) {
        return false;
    }

    *up = (((int32_t)data[0] << 16) | ((int32_t)data[1] << 8) | data[2]) >> 8;

    return true;
}

static int32_t bmp_calculate_b5(int32_t ut, const BMP180Calibration* c) {
    int32_t X1 = ((ut - (int32_t)c->AC6) * (int32_t)c->AC5) / 32768;

    if((X1 + c->MD) == 0) {
        return 0;
    }

    int32_t X2 = ((int32_t)c->MC * 2048) / (X1 + c->MD);

    return X1 + X2;
}

int32_t get_temperature(void) {
    if(!calib_loaded) return 0;

    int32_t ut;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(!bmp_read_raw_temperature(&ut)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        return 0;
    }

    int32_t B5 = bmp_calculate_b5(ut, &calib);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return (B5 + 8) / 16;
}

int32_t get_pressure(void) {
    if(!calib_loaded) return 0;

    int32_t UT;
    int32_t UP;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(!bmp_read_raw_temperature(&UT) || !bmp_read_raw_pressure(&UP)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        return 0;
    }

    int32_t B5 = bmp_calculate_b5(UT, &calib);
    int32_t B6 = B5 - 4000;

    int32_t X1 = (calib.B2 * ((B6 * B6) >> 12)) >> 11;
    int32_t X2 = (calib.AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;

    int32_t B3 = ((((int32_t)calib.AC1 * 4) + X3) + 2) >> 2;

    X1 = (calib.AC3 * B6) >> 13;
    X2 = (calib.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;

    uint32_t B4 = (calib.AC4 * (uint32_t)(X3 + 32768)) >> 15;

    if(B4 == 0) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        return 0;
    }

    uint32_t B7 = ((uint32_t)UP - B3) * 50000UL;

    int32_t P;

    if(B7 < 0x80000000) {
        P = (B7 << 1) / B4;
    } else {
        P = (B7 / B4) << 1;
    }

    X1 = (P >> 8) * (P >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * P) >> 16;

    P += (X1 + X2 + 3791) >> 4;

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return P; // Па
}
