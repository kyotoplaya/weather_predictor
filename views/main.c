#include "main.h"

void draw_main_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    canvas_clear(canvas);

    char buf[32];

    canvas_set_font(canvas, FontBigNumbers);

    snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
    canvas_draw_str(canvas, 5, 24, buf);

    canvas_set_font(canvas, FontPrimary);

    snprintf(buf, sizeof(buf), "%02d.%02d", dt.day, dt.month);
    canvas_draw_str(canvas, 70, 24, buf);

    snprintf(buf, sizeof(buf), "%d.%d C", m->temperature / 10, m->temperature % 10);
    canvas_draw_str(canvas, 5, 42, buf);

    snprintf(buf, sizeof(buf), "%d mmHg", m->pressure);
    canvas_draw_str(canvas, 5, 56, buf);

    snprintf(buf, sizeof(buf), "%d %%", m->humidity);
    canvas_draw_str(canvas, 102, 42, buf);

    // Временное решение ввиду отсутствия датчика CO2
    canvas_draw_str(canvas, 80, 56, "407 ppm");
}
