#include "history.h"

static void shift_left(int* array, int size) {
    for(int i = 0; i < size - 1; i++) {
        array[i] = array[i + 1];
    }
}

void history_init(PredictorHistory* history, int temperature, int pressure, int humidity) {
    history->temperature = temperature;
    history->pressure = pressure;
    history->humidity = humidity;

    history->hour_index = 0;
    history->graph_interval = GraphInterval1H;

    for(int i = 0; i < HISTORY_1H_SIZE; i++) {
        history->temperature_1h[i] = temperature;
        history->pressure_1h[i] = pressure;
        history->humidity_1h[i] = humidity;
    }

    for(int i = 0; i < HISTORY_3H_SIZE; i++) {
        history->temperature_3h[i] = temperature;
        history->pressure_3h[i] = pressure;
        history->humidity_3h[i] = humidity;
    }

    for(int i = 0; i < HISTORY_DAY_SIZE; i++) {
        history->temperature_day[i] = temperature;
        history->pressure_day[i] = pressure;
        history->humidity_day[i] = humidity;
    }
}

void history_push(PredictorHistory* history, int temperature, int pressure, int humidity) {
    shift_left(history->temperature_1h, HISTORY_1H_SIZE);
    shift_left(history->pressure_1h, HISTORY_1H_SIZE);
    shift_left(history->humidity_1h, HISTORY_1H_SIZE);

    history->temperature = temperature;
    history->pressure = pressure;
    history->humidity = humidity;

    history->temperature_1h[HISTORY_1H_SIZE - 1] = temperature;
    history->pressure_1h[HISTORY_1H_SIZE - 1] = pressure;
    history->humidity_1h[HISTORY_1H_SIZE - 1] = humidity;
}

void history_get_average(PredictorHistory* history, int* averageT, int* averageP, int* averageH) {
    *averageT = 0;
    *averageP = 0;
    *averageH = 0;

    for(int i = 0; i < HISTORY_1H_SIZE; i++) {
        *averageT += history->temperature_1h[i];
        *averageP += history->pressure_1h[i];
        *averageH += history->humidity_1h[i];
    }

    *averageT /= HISTORY_1H_SIZE;
    *averageP /= HISTORY_1H_SIZE;
    *averageH /= HISTORY_1H_SIZE;
}

void history_refresh_3h(PredictorHistory* history) {
    for(int i = HISTORY_3H_SIZE - 1; i > 0; i--) {
        history->temperature_3h[i] = history->temperature_3h[i - 1];
        history->pressure_3h[i] = history->pressure_3h[i - 1];
        history->humidity_3h[i] = history->humidity_3h[i - 1];
    }

    int averageT;
    int averageP;
    int averageH;

    history_get_average(history, &averageT, &averageP, &averageH);

    history->temperature_3h[0] = averageT;
    history->pressure_3h[0] = averageP;
    history->humidity_3h[0] = averageH;

    if(history->hour_index < HISTORY_3H_SIZE) {
        history->hour_index++;
    }
}

void history_refresh_day(PredictorHistory* history) {
    shift_left(history->temperature_day, HISTORY_DAY_SIZE);
    shift_left(history->pressure_day, HISTORY_DAY_SIZE);
    shift_left(history->humidity_day, HISTORY_DAY_SIZE);

    int averageT;
    int averageP;
    int averageH;

    history_get_average(history, &averageT, &averageP, &averageH);

    history->temperature_day[HISTORY_DAY_SIZE - 1] = averageT;
    history->pressure_day[HISTORY_DAY_SIZE - 1] = averageP;
    history->humidity_day[HISTORY_DAY_SIZE - 1] = averageH;
}

void history_save(PredictorHistory* history) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(
           file, "/ext/apps_data/predictor/history.bin", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, history, sizeof(PredictorHistory));
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

void history_load(PredictorHistory* history) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(
           file, "/ext/apps_data/predictor/history.bin", FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, history, sizeof(PredictorHistory));
    } else {
        // если файла нет — инициализация
        memset(history, 0, sizeof(PredictorHistory));
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}
