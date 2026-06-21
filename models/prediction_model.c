#include "prediction_model.h"

void predictor_model_init(PredictorModel* model, PredictorHistory* history) {
    SensorData data;
    sensors_read(&data);

    model->temperature = data.temperature;
    model->pressure = data.pressure;
    model->humidity = data.humidity;
    model->history = history;

    history_init(history, data.temperature, data.pressure, data.humidity);
}

void predictor_model_update(PredictorModel* model, const SensorData* data) {
    model->temperature = data->temperature;
    model->pressure = data->pressure;
    model->humidity = data->humidity;
}
