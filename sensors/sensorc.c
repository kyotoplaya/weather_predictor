#include <furi.h>

#include "sensors.h"
#include "../bme280/bme280.h"
#include "co2.h"

#define CO2_REFRESH_MS 10000

static int co2_last = 407;
static uint32_t co2_last_tick = 0;
static bool co2_first = true;

bool sensors_init(void) {
    bool ok = bme280_init();

    if(!co2_init()) {
        ok = false;
    }

    return ok;
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

    /* CO2 обновляется реже: газ меняется медленно, а опрос по UART не бесплатный */
    const uint32_t now = furi_get_tick();

    if(co2_first || now - co2_last_tick >= furi_ms_to_ticks(CO2_REFRESH_MS)) {
        int ppm;
        if(co2_read(&ppm)) {
            co2_last = ppm;
        }
        co2_last_tick = now;
        co2_first = false;
    }

    data->co2 = co2_last;

    return true;
}
