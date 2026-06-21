#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>

#include "bme280/bme280.h"
#include "utils/utils.h"
#include "history/history.h"
#include "sensors/sensors.h"
#include "graph/graph.h"
#include "prediction/prediction.h"

#include "views/main.h"
#include "views/graph.h"

#define TAG "Predictor"

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

static uint8_t next_view(uint8_t v) {
    return (v + 1) % PredictorViewCount;
}

static uint8_t prev_view(uint8_t v) {
    return (v + PredictorViewCount - 1) % PredictorViewCount;
}

static uint32_t predictor_exit_navigation_callback(void* context) {
    PredictorApp* app = context;
    history_save(app->history);
    return VIEW_NONE;
}

static uint32_t predictor_back_to_main_callback(void* context) {
    PredictorApp* app = context;

    app->selected_view_index = MainView;

    return MainView;
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

static bool predictor_custom_event_callback(uint32_t event, void* context) {
    PredictorApp* app = context;

    SensorData data;
    sensors_read(&data);

    data.temperature = data.temperature;
    data.pressure = data.pressure;
    data.humidity = data.humidity;

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
        history_refresh_day(app->history);

        history_refresh_3h(app->history);

        weather_prediction_get(app->history, app->prediction_text, 256);
        text_box_set_text(app->prediction_textbox, app->prediction_text);

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
        app->selected_view_index = 0;
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

    SensorData data;
    sensors_read(&data);

    app->history = calloc(1, sizeof(PredictorHistory));
    history_load(app->history);

    with_view_model(
        app->main_view,
        PredictorModel * model,
        {
            model->temperature = data.temperature;
            model->pressure = data.pressure;
            model->humidity = data.humidity;
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
