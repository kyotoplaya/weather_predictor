#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "bme280/bme280.h"
#include "utils/utils.h"

#define TAG "Predictor"

#define LIST_SIZE 15

typedef enum {
    MainView,
    TempGraphView,
    PressureGraphView,
} PredictorView;

typedef enum {
    TimeEventRedraw = 0,
    BMEEventRedraw = 1,
} PredictorEvent;

typedef struct {
    ViewDispatcher* view_dispatcher;

    View* main_view;
    View* temp_graph_view;
    View* pressure_graph_view;

    FuriTimer* main_view_timer;
    FuriTimer* graph_timer;
    FuriTimer* analyzer_timer;

} PredictorApp;

typedef struct {
    int temperature;
    int pressure;
    int humidity;

    int temperature_1h_list[15];
    int pressure_1h_list[15];

    int temperature_24h_list[24];
    int pressure_24h_list[24];
} PredictorModel;

static void predictor_model_init(PredictorModel* model) {
    int32_t temp, press;
    int rh;
    bme280_read(&temp, &press, &rh);

    model->temperature = temp;
    model->pressure = (int)(press / 133.3);
    model->humidity = rh;

    for(int i = 0; i < 15; i++) {
        model->temperature_1h_list[i] = model->temperature;
        model->pressure_1h_list[i] = model->pressure;
    }

    for(int i = 0; i < 24; i++) {
        model->temperature_24h_list[i] = model->temperature;
        model->pressure_24h_list[i] = model->pressure;
    }
}

static void predictor_model_update(PredictorModel* model) {
    int32_t temp, press;
    int rh;
    bme280_read(&temp, &press, &rh);

    model->temperature = temp;
    model->pressure = (int)(press / 133.3);
    model->humidity = rh;

    for(int i = 0; i < 14; i++) {
        model->temperature_1h_list[i] = model->temperature_1h_list[i + 1];

        model->pressure_1h_list[i] = model->pressure_1h_list[i + 1];
    }

    model->temperature_1h_list[14] = model->temperature;
    model->pressure_1h_list[14] = model->pressure;
}

static uint32_t predictor_exit_navigation_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t predictor_back_to_main_callback(void* context) {
    UNUSED(context);
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

static void draw_temp_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    int min = find_min(m->temperature_1h_list) / 10;
    int max = find_max(m->temperature_1h_list) / 10;

    char buf[32];

    canvas_set_font(canvas, FontPrimary);

    snprintf(buf, sizeof(buf), "%2d", max);
    canvas_draw_str(canvas, 110, 10, buf);

    snprintf(buf, sizeof(buf), "%2d", (int)m->temperature / 10);
    canvas_draw_str(canvas, 110, 35, buf);

    snprintf(buf, sizeof(buf), "%2d", min);
    canvas_draw_str(canvas, 110, 60, buf);

    int x1 = 5;
    int y2;

    for(int i = 0; i < LIST_SIZE; i++) {
        y2 = ((int)(m->temperature_1h_list[i] / 10 - min)) * 6 + 10;
        if(y2 <= 10) y2 = 10;

        canvas_draw_box(canvas, x1, 60 - y2, 5, y2);
        x1 += 7;
    }
}

static void draw_pressure_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    int min = find_min(m->pressure_1h_list);
    int max = find_max(m->pressure_1h_list);

    char buf[32];

    canvas_set_font(canvas, FontPrimary);

    snprintf(buf, sizeof(buf), "%d", max);
    canvas_draw_str(canvas, 100, 10, buf);

    snprintf(buf, sizeof(buf), "%d", (int)m->pressure);
    canvas_draw_str(canvas, 100, 35, buf);

    snprintf(buf, sizeof(buf), "%d", min);
    canvas_draw_str(canvas, 100, 60, buf);

    int x1 = 5;
    int y2;

    for(int i = 0; i < LIST_SIZE; i++) {
        y2 = ((int)(m->pressure_1h_list[i] / 10 - min)) * 6 + 10;
        if(y2 <= 10) y2 = 10;

        canvas_draw_box(canvas, x1, 60 - y2, 5, y2);
        x1 += 6;
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

static void main_view_enter_callback(void* context) {
    PredictorApp* app = context;

    furi_assert(app->main_view_timer == NULL);

    app->main_view_timer = furi_timer_alloc(main_view_timer_callback, FuriTimerTypePeriodic, app);

    furi_timer_start(app->main_view_timer,
                     furi_ms_to_ticks(2000)); // 1 минута
}

static void graph_view_enter_callback(void* context) {
    PredictorApp* app = context;

    furi_assert(app->graph_timer == NULL);

    app->graph_timer = furi_timer_alloc(graph_timer_callback, FuriTimerTypePeriodic, app);

    furi_timer_start(app->graph_timer,
                     furi_ms_to_ticks(10000)); // 10 сек
}

static void main_view_exit_callback(void* context) {
    PredictorApp* app = context;

    if(app->main_view_timer) {
        furi_timer_stop(app->main_view_timer);
        furi_timer_free(app->main_view_timer);

        app->main_view_timer = NULL;
    }
}

static void graph_view_exit_callback(void* context) {
    PredictorApp* app = context;

    if(app->graph_timer) {
        furi_timer_stop(app->graph_timer);
        furi_timer_free(app->graph_timer);

        app->graph_timer = NULL;
    }
}

static bool predictor_custom_event_callback(uint32_t event, void* context) {
    PredictorApp* app = context;

    switch(event) {
    case TimeEventRedraw:
        int32_t temp, press;
        int rh;
        bme280_read(&temp, &press, &rh);
        with_view_model(
            app->main_view,
            PredictorModel * model,
            {
                model->temperature = temp;
                model->pressure = (int)(press / 133.3);
                model->humidity = rh;
            },
            true);
        return true;

    case BMEEventRedraw:

        with_view_model(
            app->main_view, PredictorModel * model, { predictor_model_update(model); }, true);

        with_view_model(
            app->temp_graph_view, PredictorModel * model, { predictor_model_update(model); }, true);

        with_view_model(
            app->pressure_graph_view,
            PredictorModel * model,
            { predictor_model_update(model); },
            true);

        return true;
    }
    return false;
}

static bool predictor_input_callback(InputEvent* event, void* context) {
    PredictorApp* app = (PredictorApp*)context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyRight) {
            view_dispatcher_switch_to_view(app->view_dispatcher, TempGraphView);
        } else if(event->key == InputKeyLeft) {
            view_dispatcher_switch_to_view(app->view_dispatcher, PressureGraphView);
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

    view_set_context(app->main_view, app);
    view_set_context(app->temp_graph_view, app);
    view_set_context(app->pressure_graph_view, app);

    // Аллоцируем модели
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    view_allocate_model(app->temp_graph_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    view_allocate_model(app->pressure_graph_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    int32_t temp, press;
    int rh;
    bme280_read(&temp, &press, &rh);

    with_view_model(
        app->main_view,
        PredictorModel * model,
        {
            model->temperature = temp;
            model->pressure = (int)(press / 133.3);
            model->humidity = rh;

            for(int i = 0; i < 15; i++) {
                model->temperature_1h_list[i] = model->temperature;
                model->pressure_1h_list[i] = model->pressure;
            }

            for(int i = 0; i < 24; i++) {
                model->temperature_24h_list[i] = model->temperature;
                model->pressure_24h_list[i] = model->pressure;
            }
        },
        false);

    with_view_model(
        app->main_view, PredictorModel * model, { predictor_model_init(model); }, false);

    with_view_model(
        app->temp_graph_view, PredictorModel * model, { predictor_model_init(model); }, false);

    with_view_model(
        app->pressure_graph_view, PredictorModel * model, { predictor_model_init(model); }, false);

    view_set_draw_callback(app->main_view, draw_main_callback);
    view_set_draw_callback(app->temp_graph_view, draw_temp_graph_callback);
    view_set_draw_callback(app->pressure_graph_view, draw_pressure_graph_callback);

    view_set_input_callback(app->main_view, predictor_input_callback);
    view_set_input_callback(app->temp_graph_view, predictor_input_callback);
    view_set_input_callback(app->pressure_graph_view, predictor_input_callback);

    view_set_enter_callback(app->main_view, main_view_enter_callback);
    view_set_enter_callback(app->temp_graph_view, graph_view_enter_callback);
    view_set_enter_callback(app->pressure_graph_view, graph_view_enter_callback);

    view_set_exit_callback(app->main_view, main_view_exit_callback);
    view_set_exit_callback(app->temp_graph_view, graph_view_exit_callback);
    view_set_exit_callback(app->pressure_graph_view, graph_view_exit_callback);

    view_set_previous_callback(app->main_view, predictor_exit_navigation_callback);
    view_set_previous_callback(app->temp_graph_view, predictor_back_to_main_callback);
    view_set_previous_callback(app->pressure_graph_view, predictor_back_to_main_callback);

    view_set_custom_callback(app->main_view, predictor_custom_event_callback);
    view_set_custom_callback(app->temp_graph_view, predictor_custom_event_callback);
    view_set_custom_callback(app->pressure_graph_view, predictor_custom_event_callback);

    view_dispatcher_add_view(app->view_dispatcher, MainView, app->main_view);
    view_dispatcher_add_view(app->view_dispatcher, TempGraphView, app->temp_graph_view);
    view_dispatcher_add_view(app->view_dispatcher, PressureGraphView, app->pressure_graph_view);

    view_dispatcher_switch_to_view(app->view_dispatcher, MainView);

    return app;
}

static void predictor_app_free(PredictorApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, MainView);
    view_dispatcher_remove_view(app->view_dispatcher, TempGraphView);
    view_dispatcher_remove_view(app->view_dispatcher, PressureGraphView);

    view_free(app->main_view);
    view_free(app->temp_graph_view);
    view_free(app->pressure_graph_view);

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
