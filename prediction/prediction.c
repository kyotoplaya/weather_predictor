#include "prediction.h"

void weather_prediction_get(PredictorHistory* history, char* text, int text_size) {
    char buf[256];

    if(history->hour_index < 3) {
        snprintf(
            buf, sizeof(buf), "Wait %d hours for prediction, please.", 3 - history->hour_index);

    } else {
        int dp1, dp2, dp3, accP, dh, trendH, dt;
        int p0, p1, p2, p3, h0, h3, t0, t3;
        int rainScore;

        p0 = history->pressure;
        p1 = history->pressure_3h[0];
        p2 = history->pressure_3h[1];
        p3 = history->pressure_3h[2];

        h0 = history->humidity;
        h3 = history->humidity_3h[2];

        t0 = history->temperature;
        t3 = history->temperature_3h[2];

        dp1 = p0 - p1;
        dp2 = p1 - p2;
        dp3 = p2 - p3;

        accP = (dp1 - dp2) + (dp2 - dp3);

        dh = h0 - h3;
        trendH = (h0 - h3) / 3;

        dt = t0 - t3;

        if(dt < 0) dt *= -1;

        rainScore = (-dp1 * 4) + (-accP * 3) + (dh * 2) + (trendH * 1.5) + (dt * 0.5);

        if(rainScore < 0) rainScore = 0;
        if(rainScore > 100) rainScore = 100;

        if(rainScore >= 0 && rainScore <= 20)
            snprintf(buf, sizeof(buf), "Clear weather is expected.");

        if(rainScore > 20 && rainScore <= 40)
            snprintf(buf, sizeof(buf), "Cloudy weather is expected.");

        if(rainScore > 40 && rainScore <= 65) snprintf(buf, sizeof(buf), "Rain is possible.");

        if(rainScore > 65 && rainScore <= 85) snprintf(buf, sizeof(buf), "It should rain now.");

        if(rainScore > 85) snprintf(buf, sizeof(buf), "Heavy rain/storm expected.");
    }

    snprintf(text, text_size, "%s", buf);
}
