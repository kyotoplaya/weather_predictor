#pragma once

#include "../history/history.h"
#include "../sensors/sensors.h"

typedef struct {
    int temperature;
    int pressure;
    int humidity;

    PredictorHistory* history;
} PredictorModel;

void predictor_model_init(PredictorModel* model, PredictorHistory* history);

void predictor_model_update(PredictorModel* model, const SensorData* data);
