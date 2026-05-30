#pragma once

#include <stdint.h>
#include <stdbool.h>

extern const uint8_t bmp_addr;

bool bmp180_init(void);

/**
 * Возвращает температуру в десятых долях градуса.
 * Например:
 *   253 -> 25.3°C
 *   217 -> 21.7°C
 */
int32_t get_temperature(void);

/**
 * Возвращает давление в Паскалях.
 * Например:
 *   101325 -> 101325 Па
 */
int32_t get_pressure(void);
