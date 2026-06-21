#include "sensors.h"
#include "../bme280/bme280.h"

bool sensors_init(void) {
    return bme280_init();
}

bool sensors_read(SensorData* data) {
    if(!data) {
        return false;
    }

    int32_t temp;
    int32_t press;
    int rh;

    bme280_read(&temp, &press, &rh);

    data->temperature = temp;
    data->pressure = press / 133.3;
    data->humidity = rh;

    // пока датчика нет
    data->co2 = 407;

    return true;
}
