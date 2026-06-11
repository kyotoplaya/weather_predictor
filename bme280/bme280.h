#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Init BME280 sensor:
 * - reads calibration
 * - configures forced mode
 */
bool bme280_init(void);

/**
 * Single measurement snapshot.
 *
 * IMPORTANT:
 * Temperature and pressure are from the same measurement cycle.
 *
 * @param temp  output temperature in 0.01°C (e.g. 3123 = 31.23°C)
 * @param press output pressure in Pa
 */
bool bme280_read(int32_t* temp, int32_t* press, int* rh);

#ifdef __cplusplus
}
#endif
