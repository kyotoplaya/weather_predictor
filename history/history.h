#pragma once

#include <stdint.h>

#include <storage/storage.h>

#define HISTORY_1H_SIZE  15
#define HISTORY_3H_SIZE  3
#define HISTORY_DAY_SIZE 24

typedef enum {
    GraphInterval1H,
    GraphInterval24H,
} GraphInterval;

typedef struct {
    int temperature;
    int pressure;
    int humidity;

    int temperature_1h[HISTORY_1H_SIZE];
    int pressure_1h[HISTORY_1H_SIZE];
    int humidity_1h[HISTORY_1H_SIZE];

    int temperature_3h[HISTORY_3H_SIZE];
    int pressure_3h[HISTORY_3H_SIZE];
    int humidity_3h[HISTORY_3H_SIZE];

    int temperature_day[HISTORY_DAY_SIZE];
    int pressure_day[HISTORY_DAY_SIZE];
    int humidity_day[HISTORY_DAY_SIZE];

    int8_t hour_index;

    GraphInterval graph_interval;
} PredictorHistory;

void history_init(PredictorHistory* history, int temperature, int pressure, int humidity);

void history_push(PredictorHistory* history, int temperature, int pressure, int humidity);

void history_get_average(PredictorHistory* history, int* averageT, int* averageP, int* averageH);

void history_refresh_3h(PredictorHistory* history);

void history_refresh_day(PredictorHistory* history);

void history_load(PredictorHistory* history);

void history_save(PredictorHistory* history);
