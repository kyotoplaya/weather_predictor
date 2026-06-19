#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>

#include "bme280/bme280.h"
#include "utils/utils.h"

#define TAG "Predictor"

#define LIST_SIZE 15

typedef enum {
    MainView,
    TempGraphView,
    PressureGraphView,
    HumidityGraphView,
    PredictorViewCount, // Не менять индекс данной строки в enum для корректной работы кругового меню

    PredictionTextBoxView,

} PredictorView;

typedef enum {
    TimeEventRedraw,
    BMEEventRedraw,
    HistoryEvent,
    GraphIntervalChangeEvent,
} PredictorEvent;

typedef enum {
    GraphInterval1H,
    GraphInterval24H,
} GraphInterval;

typedef struct {
    int temperature;
    int pressure;
    int humidity;

    int temperature_1h[15]; // измерения за час, 15 с интервалом 4 мин
    int pressure_1h[15];
    int humidity_1h[15];

    int temperature_3h[3]; // усредненные измерения за 3 часа с интервалом час
    int pressure_3h[3];
    int humidity_3h[3];

    int temperature_day[24]; // усредненные измерения за сутки с интервалом час
    int pressure_day[24];
    int humidity_day[24];

    int8_t hour_index;

    GraphInterval graph_interval;

} PredictorHistory;

typedef struct {
    ViewDispatcher* view_dispatcher;

    View* main_view;
    View* temp_graph_view;
    View* pressure_graph_view;
    View* humidity_graph_view;

    TextBox* prediction_textbox;

    FuriTimer* main_view_timer;
    FuriTimer* graph_timer;
    FuriTimer* history_timer;

    int8_t selected_view_index;

    PredictorHistory* history;

    char prediction_text[256];

} PredictorApp;

typedef struct {
    int temperature;
    int pressure;
    int humidity;

    PredictorHistory* history;

} PredictorModel;

typedef struct {
    int temperature;
    int pressure;
    int humidity;
} SensorData;

static uint8_t next_view(uint8_t v) {
    return (v + 1) % PredictorViewCount;
}

static uint8_t prev_view(uint8_t v) {
    return (v + PredictorViewCount - 1) % PredictorViewCount;
}

static void history_push(PredictorHistory* history, int temperature, int pressure, int humidity) {
    for(int i = 0; i < 14; i++) {
        history->temperature_1h[i] = history->temperature_1h[i + 1];

        history->pressure_1h[i] = history->pressure_1h[i + 1];

        history->humidity_1h[i] = history->humidity_1h[i + 1];
    }

    history->temperature = temperature;
    history->pressure = pressure;
    history->humidity = humidity;

    history->temperature_1h[14] = temperature;
    history->pressure_1h[14] = pressure;
    history->humidity_1h[14] = humidity;
}

static void predictor_model_init(PredictorModel* model, PredictorHistory* history) {
    int32_t temp, press;
    int rh;
    bme280_read(&temp, &press, &rh);

    model->temperature = temp;
    model->pressure = (int)(press / 133.3);
    model->humidity = rh;
    model->history = history;

    for(int i = 0; i < 15; i++) {
        model->history->temperature_1h[i] = model->temperature;
        model->history->pressure_1h[i] = model->pressure;
        model->history->humidity_1h[i] = model->humidity;
    }

    for(int i = 0; i < 24; i++) {
        model->history->temperature_day[i] = model->temperature;
        model->history->pressure_day[i] = model->pressure;
        model->history->humidity_day[i] = model->humidity;
    }
}

static void predictor_model_update(PredictorModel* model, const SensorData* data) {
    model->temperature = data->temperature;
    model->pressure = data->pressure;
    model->humidity = data->humidity;
}

static uint32_t predictor_exit_navigation_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t predictor_back_to_main_callback(void* context) {
    PredictorApp* app = context;

    app->selected_view_index = MainView;

    return MainView;
}

static void draw_main_callback(Canvas* canvas, void* model) {
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

static void draw_bar_graph(
    Canvas* canvas,
    const int* data,
    size_t count,
    int current_value,
    int divisor,
    const char* title,
    bool show_current) {
    int min_raw = find_min((int*)data, count);
    int max_raw = find_max((int*)data, count);

    int range = max_raw - min_raw;
    if(range == 0) {
        range = 1;
    }

    char buf[32];

    canvas_set_font(canvas, FontPrimary);

    if(show_current) {
        snprintf(buf, sizeof(buf), "%d", max_raw / divisor);
        canvas_draw_str(canvas, 110, 10, buf);

        snprintf(buf, sizeof(buf), "%d", current_value / divisor);
        canvas_draw_str(canvas, 110, 35, buf);

        snprintf(buf, sizeof(buf), "%d", min_raw / divisor);
        canvas_draw_str(canvas, 110, 60, buf);
    } else { // 24h режим: только min и max сверху
        snprintf(buf, sizeof(buf), "%d", min_raw / divisor);
        canvas_draw_str(canvas, 2, 10, buf);

        snprintf(buf, sizeof(buf), "%d", max_raw / divisor);

        uint8_t w = canvas_string_width(canvas, buf);
        canvas_draw_str(canvas, 126 - w, 10, buf);
    }

    if(title) {
        if(show_current)
            canvas_draw_str(canvas, 2, 10, title);
        else
            canvas_draw_str(canvas, 58, 10, title);
    }

    int step = (count == 24) ? 5 : 7;
    int x = 5;

    int graph_height = show_current ? 50 : 36;
    int graph_bottom = 63;

    for(size_t i = 0; i < count; i++) {
        int height = ((data[i] - min_raw) * graph_height) / range + 2;

        canvas_draw_box(canvas, x, graph_bottom - height, 5, height);

        x += step;
    }
}

static void draw_temp_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    if(m->history->graph_interval == GraphInterval1H) {
        draw_bar_graph(canvas, m->history->temperature_1h, 15, m->temperature, 10, "1h", true);
    } else {
        draw_bar_graph(canvas, m->history->temperature_day, 24, m->temperature, 10, "24h", false);
    }
}

static void draw_pressure_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    if(m->history->graph_interval == GraphInterval1H) {
        draw_bar_graph(canvas, m->history->pressure_1h, 15, m->pressure, 1, "1h", true);
    } else {
        draw_bar_graph(canvas, m->history->pressure_day, 24, m->pressure, 1, "24h", false);
    }
}

static void draw_humidity_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    if(m->history->graph_interval == GraphInterval1H) {
        draw_bar_graph(canvas, m->history->humidity_1h, 15, m->humidity, 1, "1h", true);
    } else {
        draw_bar_graph(canvas, m->history->humidity_day, 24, m->humidity, 1, "24h", false);
    }
}

static void main_view_timer_callback(void* context) {
    PredictorApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, TimeEventRedraw);
}

static void graph_timer_callback(void* context) {
    PredictorApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, BMEEventRedraw);
}

static void history_timer_callback(void* context) {
    PredictorApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, HistoryEvent);
}

static void predictor_update_all_models(PredictorApp* app, const SensorData* data) {
    with_view_model(
        app->main_view, PredictorModel * model, { predictor_model_update(model, data); }, true);

    with_view_model(
        app->temp_graph_view,
        PredictorModel * model,
        { predictor_model_update(model, data); },
        true);

    with_view_model(
        app->pressure_graph_view,
        PredictorModel * model,
        { predictor_model_update(model, data); },
        true);

    with_view_model(
        app->humidity_graph_view,
        PredictorModel * model,
        { predictor_model_update(model, data); },
        true);
}

static void get_average_readings(void* context, int* averageT, int* averageP, int* averageH) {
    PredictorApp* app = context;

    for(int i = 0; i < LIST_SIZE; i++) {
        *averageT += app->history->temperature_1h[i];
        *averageP += app->history->pressure_1h[i];
        *averageH += app->history->humidity_1h[i];
    }

    *averageT /= LIST_SIZE;
    *averageP /= LIST_SIZE;
    *averageH /= LIST_SIZE;
}

static void refresh_3h_arrays(void* context) {
    PredictorApp* app = context;

    // Сдвиг истории на 1 элемент вправо
    for(int i = 2; i > 0; i--) {
        app->history->temperature_3h[i] = app->history->temperature_3h[i - 1];

        app->history->pressure_3h[i] = app->history->pressure_3h[i - 1];

        app->history->humidity_3h[i] = app->history->humidity_3h[i - 1];
    }

    int averageT = 0;
    int averageP = 0;
    int averageH = 0;

    get_average_readings(app, &averageT, &averageP, &averageH);

    // Новое значение всегда в начало массива
    app->history->temperature_3h[0] = averageT;
    app->history->pressure_3h[0] = averageP;
    app->history->humidity_3h[0] = averageH;

    // Счётчик заполнения истории
    if(app->history->hour_index < 3) {
        app->history->hour_index++;
    }
}

static void refresh_day_arrays(void* context) {
    PredictorApp* app = context;

    for(int i = 0; i < 23; i++) {
        app->history->temperature_day[i] = app->history->temperature_day[i + 1];
        app->history->pressure_day[i] = app->history->pressure_day[i + 1];
        app->history->humidity_day[i] = app->history->humidity_day[i + 1];
    }

    int averageT = 0;
    int averageP = 0;
    int averageH = 0;

    get_average_readings(app, &averageT, &averageP, &averageH);

    app->history->temperature_day[23] = averageT;
    app->history->pressure_day[23] = averageP;
    app->history->humidity_day[23] = averageH;
}

static void get_prediction(void* context) {
    PredictorApp* app = context;

    char buf[256];

    if(app->history->hour_index < 3) {
        snprintf(
            buf,
            sizeof(buf),
            "Wait %d hours for prediction, please.",
            3 - app->history->hour_index);

    } else {
        int dp1, dp2, dp3, accP, dh, trendH, dt;
        int p0, p1, p2, p3, h0, h3, t0, t3;
        int rainScore;

        p0 = app->history->pressure;
        p1 = app->history->pressure_3h[0];
        p2 = app->history->pressure_3h[1];
        p3 = app->history->pressure_3h[2];

        h0 = app->history->humidity;
        h3 = app->history->humidity_3h[2];

        t0 = app->history->temperature;
        t3 = app->history->temperature_3h[2];

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

    strncpy(app->prediction_text, buf, sizeof(app->prediction_text) - 1);
    app->prediction_text[sizeof(app->prediction_text) - 1] = '\0';

    text_box_set_text(app->prediction_textbox, app->prediction_text);
}

static bool predictor_custom_event_callback(uint32_t event, void* context) {
    PredictorApp* app = context;

    SensorData data;

    int32_t temp, press;
    int rh;

    bme280_read(&temp, &press, &rh);

    data.temperature = temp;
    data.pressure = press / 133.3;
    data.humidity = rh;

    switch(event) {
    case TimeEventRedraw:
        with_view_model(
            app->main_view,
            PredictorModel * model,
            { predictor_model_update(model, &data); },
            true);
        return true;

    case BMEEventRedraw:
        history_push(app->history, data.temperature, data.pressure, data.humidity);

        predictor_update_all_models(app, &data);

        return true;

    case HistoryEvent:
        refresh_day_arrays(app);

        refresh_3h_arrays(app);
        get_prediction(app);

        return true;

    case GraphIntervalChangeEvent:
        with_view_model(app->temp_graph_view, PredictorModel * model, { UNUSED(model); }, true);
        with_view_model(
            app->pressure_graph_view, PredictorModel * model, { UNUSED(model); }, true);
        with_view_model(
            app->humidity_graph_view, PredictorModel * model, { UNUSED(model); }, true);

        return true;
    }

    return false;
}

static bool predictor_input_callback(InputEvent* event, void* context) {
    PredictorApp* app = context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyRight) {
        app->selected_view_index = next_view(app->selected_view_index);
        view_dispatcher_switch_to_view(app->view_dispatcher, app->selected_view_index);

        return true;
    }

    if(event->key == InputKeyLeft) {
        app->selected_view_index = prev_view(app->selected_view_index);
        view_dispatcher_switch_to_view(app->view_dispatcher, app->selected_view_index);

        return true;
    }

    if(event->key == InputKeyUp) {
        view_dispatcher_switch_to_view(app->view_dispatcher, PredictionTextBoxView);

        return true;
    }

    if(event->key == InputKeyOk) {
        if(app->selected_view_index != 0) {
            if(app->history->graph_interval == GraphInterval1H)
                app->history->graph_interval = GraphInterval24H;
            else
                app->history->graph_interval = GraphInterval1H;

            view_dispatcher_send_custom_event(app->view_dispatcher, GraphIntervalChangeEvent);
        }

        return true;
    }

    return false;
}

static PredictorApp* predictor_app_alloc(void) {
    PredictorApp* app = calloc(1, sizeof(PredictorApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    if(!bme280_init()) {
        FURI_LOG_E(TAG, "BME280 init failed");
    }

    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->main_view = view_alloc();
    app->temp_graph_view = view_alloc();
    app->pressure_graph_view = view_alloc();
    app->humidity_graph_view = view_alloc();

    app->prediction_textbox = text_box_alloc();
    text_box_set_text(app->prediction_textbox, "Wait 3 hours for prediction, please.");

    view_set_context(app->main_view, app);
    view_set_context(app->temp_graph_view, app);
    view_set_context(app->pressure_graph_view, app);
    view_set_context(app->humidity_graph_view, app);

    // Аллоцируем модели
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    view_allocate_model(app->temp_graph_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    view_allocate_model(app->pressure_graph_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    view_allocate_model(app->humidity_graph_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    int32_t temp, press;
    int rh;
    bme280_read(&temp, &press, &rh);

    app->history = calloc(1, sizeof(PredictorHistory));

    with_view_model(
        app->main_view,
        PredictorModel * model,
        {
            model->temperature = temp;
            model->pressure = (int)(press / 133.3);
            model->humidity = rh;
            model->history = app->history;
        },
        false);

    with_view_model(
        app->temp_graph_view,
        PredictorModel * model,
        { predictor_model_init(model, app->history); },
        false);

    with_view_model(
        app->pressure_graph_view,
        PredictorModel * model,
        { predictor_model_init(model, app->history); },
        false);

    with_view_model(
        app->humidity_graph_view,
        PredictorModel * model,
        { predictor_model_init(model, app->history); },
        false);

    view_set_draw_callback(app->main_view, draw_main_callback);
    view_set_draw_callback(app->temp_graph_view, draw_temp_graph_callback);
    view_set_draw_callback(app->pressure_graph_view, draw_pressure_graph_callback);
    view_set_draw_callback(app->humidity_graph_view, draw_humidity_graph_callback);

    view_set_input_callback(app->main_view, predictor_input_callback);
    view_set_input_callback(app->temp_graph_view, predictor_input_callback);
    view_set_input_callback(app->pressure_graph_view, predictor_input_callback);
    view_set_input_callback(app->humidity_graph_view, predictor_input_callback);

    view_set_previous_callback(app->main_view, predictor_exit_navigation_callback);
    view_set_previous_callback(app->temp_graph_view, predictor_back_to_main_callback);
    view_set_previous_callback(app->pressure_graph_view, predictor_back_to_main_callback);
    view_set_previous_callback(app->humidity_graph_view, predictor_back_to_main_callback);
    view_set_previous_callback(
        text_box_get_view(app->prediction_textbox), predictor_back_to_main_callback);

    view_set_custom_callback(app->main_view, predictor_custom_event_callback);
    view_set_custom_callback(app->temp_graph_view, predictor_custom_event_callback);
    view_set_custom_callback(app->pressure_graph_view, predictor_custom_event_callback);
    view_set_custom_callback(app->humidity_graph_view, predictor_custom_event_callback);

    view_dispatcher_add_view(app->view_dispatcher, MainView, app->main_view);
    view_dispatcher_add_view(app->view_dispatcher, TempGraphView, app->temp_graph_view);
    view_dispatcher_add_view(app->view_dispatcher, PressureGraphView, app->pressure_graph_view);
    view_dispatcher_add_view(app->view_dispatcher, HumidityGraphView, app->humidity_graph_view);

    view_dispatcher_add_view(
        app->view_dispatcher, PredictionTextBoxView, text_box_get_view(app->prediction_textbox));

    app->main_view_timer = furi_timer_alloc(main_view_timer_callback, FuriTimerTypePeriodic, app);
    app->graph_timer = furi_timer_alloc(graph_timer_callback, FuriTimerTypePeriodic, app);
    app->history_timer = furi_timer_alloc(history_timer_callback, FuriTimerTypePeriodic, app);

    app->history->hour_index = 0;
    app->history->graph_interval = GraphInterval1H;

    furi_timer_start(app->main_view_timer, furi_ms_to_ticks(60000)); // минута
    furi_timer_start(app->graph_timer, furi_ms_to_ticks(2000)); // 4 минуты
    furi_timer_start(app->history_timer, furi_ms_to_ticks(2000)); // час

    app->selected_view_index = 0;

    view_dispatcher_switch_to_view(app->view_dispatcher, app->selected_view_index);

    return app;
}

static void predictor_app_free(PredictorApp* app) {
    furi_timer_stop(app->main_view_timer);
    furi_timer_free(app->main_view_timer);

    furi_timer_stop(app->graph_timer);
    furi_timer_free(app->graph_timer);

    furi_timer_stop(app->history_timer);
    furi_timer_free(app->history_timer);

    view_dispatcher_remove_view(app->view_dispatcher, MainView);
    view_dispatcher_remove_view(app->view_dispatcher, TempGraphView);
    view_dispatcher_remove_view(app->view_dispatcher, PressureGraphView);
    view_dispatcher_remove_view(app->view_dispatcher, HumidityGraphView);
    view_dispatcher_remove_view(app->view_dispatcher, PredictionTextBoxView);

    text_box_free(app->prediction_textbox);

    view_free(app->main_view);
    view_free(app->temp_graph_view);
    view_free(app->pressure_graph_view);
    view_free(app->humidity_graph_view);

    free(app->history);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t predictor_app(void* p) {
    UNUSED(p);

    PredictorApp* app = predictor_app_alloc();

    view_dispatcher_run(app->view_dispatcher);

    predictor_app_free(app);

    return 0;
}
