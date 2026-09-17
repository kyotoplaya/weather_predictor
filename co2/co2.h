#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CO2 sensor driver (UART, 9600 8N1).
 *
 * This particular MH-Z19C module does NOT answer standard MH-Z19 commands;
 * instead it broadcasts its own 16-byte auto-report frame once per second:
 *
 *   42 4D | raw1 | raw2 | co2_raw | 01 2C | 00 00 | E2 00 | 06 | crc
 *
 * where crc = (sum of bytes 0..14) & 0xFF and co2_raw = bytes[6..7] (big-endian)
 * grows with CO2 concentration. Software converts it with a linear offset:
 *
 *   ppm = co2_raw - CO2_RAW_OFFSET   (calibrated to fresh air ~420 ppm)
 *
 * Reads are fully passive: no commands are sent.
 */
bool co2_init(void);

/** Release the serial interface. */
void co2_deinit(void);

/**
 * Single CO2 reading in ppm.
 * Blocks until the next auto-report frame arrives (up to ~1.4 s).
 * Returns false if no valid frame was received.
 */
bool co2_read(int* ppm);

#ifdef __cplusplus
}
#endif