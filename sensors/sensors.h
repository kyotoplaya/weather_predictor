#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int temperature;
    int pressure;
    int humidity;
    int co2;
} SensorData;

bool sensors_init(void);
bool sensors_read(SensorData* data);
