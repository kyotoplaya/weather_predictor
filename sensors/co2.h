#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Init CO2 sensor over UART (Flipper pins 13/14, 9600 8N1).
 * The sensor broadcasts its own 16-byte auto-report frame ~once per second,
 * so reads are fully passive (no commands sent).
 */
bool co2_init(void);

/** Send zero-calibration command (0x87). Only in fresh air. */
void co2_calibrate_zero(void);

/** Release serial interface. */
void co2_deinit(void);

/**
 * Single CO2 reading in ppm (from the periodic auto-report frame).
 * Returns false if no valid frame received within timeout.
 */
bool co2_read(int* ppm);

#ifdef __cplusplus
}
#endif