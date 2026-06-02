#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "bmp180/bmp180.h"
#include "utils/utils.h"

#define TAG "Predictor"

#define LIST_SIZE 15

#define GRAPH_X    5
#define GRAPH_Y    5
#define GRAPH_W    100
#define GRAPH_H    54
#define POINT_SIZE 3

typedef enum {
    MainView,
    TempGraphView,

} PredictorView;

typedef enum {
    TimeEventRedraw = 0,
    BMEEventRedraw = 1,
} PredictorEvent;

typedef struct {
    ViewDispatcher* view_dispatcher;

    View* main_view;
    View* temp_graph_view;

    FuriTimer* main_view_timer;
    FuriTimer* graph_timer;

    int32_t temperature;
    int32_t pressure;

} PredictorApp;

typedef struct {
    int32_t temperature;
    int32_t pressure;
} MainViewModel;

typedef struct {
    int32_t temperature;
    int32_t pressure;

    int32_t temperature_list[LIST_SIZE];
    int32_t pressure_list[LIST_SIZE];
} GraphViewModel;

static uint32_t predictor_exit_navigation_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t predictor_back_to_main_callback(void* context) {
    UNUSED(context);
    return MainView;
}

static void draw_main_callback(Canvas* canvas, void* model) {
    MainViewModel* m = model;

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

    snprintf(buf, sizeof(buf), "%ld.%ld C", m->temperature / 10, labs(m->temperature % 10));
    canvas_draw_str(canvas, 5, 42, buf);

    snprintf(buf, sizeof(buf), "%ld mmHg", m->pressure);
    canvas_draw_str(canvas, 5, 56, buf);

    // Временное решение ввиду отсутствия BME280
    canvas_draw_str(canvas, 102, 42, "27%");

    // Временное решение ввиду отсутствия датчика CO2
    canvas_draw_str(canvas, 80, 56, "407 ppm");
}

static void draw_temp_graph_callback(Canvas* canvas, void* model) {
    GraphViewModel* m = model;

    canvas_clear(canvas);

    char buf[32];

    canvas_set_font(canvas, FontPrimary);

    int min_value = find_min(m->temperature_list);
    int max_value = find_max(m->temperature_list);

    snprintf(buf, sizeof(buf), "%2d", max_value);
    canvas_draw_str(canvas, 110, 10, buf);

    snprintf(buf, sizeof(buf), "%2d", (int)m->temperature / 10);
    canvas_draw_str(canvas, 110, 35, buf);

    snprintf(buf, sizeof(buf), "%2d", min_value);
    canvas_draw_str(canvas, 110, 60, buf);

    // for(int i = 0; i < 5; i++) {
    //     snprintf(buf, sizeof(buf), "%d", m->temperature_list[i]);
    //     canvas_draw_str(canvas, 5, 11 * (i + 1), buf);
    // }

    // for(int i = 0; i < 5; i++) {
    //     snprintf(buf, sizeof(buf), "%d", m->temperature_list[i + 5]);
    //     canvas_draw_str(canvas, 50, 11 * (i + 1), buf);
    // }
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
                     furi_ms_to_ticks(60000)); // 1 минута
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
        with_view_model(
            app->main_view,
            MainViewModel * model,
            {
                model->temperature = get_temperature();
                model->pressure = (int)(get_pressure() / 133.3);
            },
            true);
        return true;
    case BMEEventRedraw:
        with_view_model(
            app->temp_graph_view,
            GraphViewModel * model,
            {
                model->temperature = get_temperature();
                model->pressure = (int)(get_pressure() / 133.3);

                for(int i = 0; i < LIST_SIZE - 1; i++) {
                    model->temperature_list[i] = model->temperature_list[i + 1];
                }
                model->temperature_list[LIST_SIZE - 1] = model->temperature;

                for(int i = 0; i < LIST_SIZE - 1; i++) {
                    model->pressure_list[i] = model->pressure_list[i + 1];
                }
                model->pressure_list[LIST_SIZE - 1] = model->pressure;
            },
            true);
        return true;

    default:
        return false;
    }
}

static bool predictor_input_callback(InputEvent* event, void* context) {
    PredictorApp* app = (PredictorApp*)context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyRight) {
            view_dispatcher_switch_to_view(app->view_dispatcher, TempGraphView);

            return true;
        }
    }
    return false;
}

static PredictorApp* predictor_app_alloc(void) {
    PredictorApp* app = calloc(1, sizeof(PredictorApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    if(!bmp180_init()) {
        FURI_LOG_E(TAG, "BMP180 init failed");
    }

    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->main_view = view_alloc();
    app->temp_graph_view = view_alloc();

    view_set_context(app->main_view, app);
    view_set_context(app->temp_graph_view, app);

    // Аллоцируем модели
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(MainViewModel));
    view_allocate_model(app->temp_graph_view, ViewModelTypeLockFree, sizeof(GraphViewModel));

    with_view_model(
        app->main_view,
        MainViewModel * model,
        {
            model->temperature = get_temperature();
            model->pressure = (int)(get_pressure() / 133.3);
        },
        false);

    with_view_model(
        app->temp_graph_view,
        GraphViewModel * model,
        {
            model->temperature = get_temperature();
            model->pressure = (int)(get_pressure() / 133.3);

            for(int i = 0; i < LIST_SIZE; i++) {
                model->temperature_list[i] = model->temperature;
                model->pressure_list[i] = model->pressure;
            }
        },
        false);

    view_set_draw_callback(app->main_view, draw_main_callback);
    view_set_draw_callback(app->temp_graph_view, draw_temp_graph_callback);

    view_set_input_callback(app->main_view, predictor_input_callback);
    view_set_input_callback(app->temp_graph_view, predictor_input_callback);

    view_set_enter_callback(app->main_view, main_view_enter_callback);
    view_set_enter_callback(app->temp_graph_view, graph_view_enter_callback);

    view_set_exit_callback(app->main_view, main_view_exit_callback);
    view_set_exit_callback(app->temp_graph_view, graph_view_exit_callback);

    view_set_previous_callback(app->main_view, predictor_exit_navigation_callback);
    view_set_previous_callback(app->temp_graph_view, predictor_back_to_main_callback);

    view_set_custom_callback(app->main_view, predictor_custom_event_callback);
    view_set_custom_callback(app->temp_graph_view, predictor_custom_event_callback);

    view_dispatcher_add_view(app->view_dispatcher, MainView, app->main_view);
    view_dispatcher_add_view(app->view_dispatcher, TempGraphView, app->temp_graph_view);

    view_dispatcher_switch_to_view(app->view_dispatcher, MainView);

    return app;
}

static void predictor_app_free(PredictorApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, MainView);
    view_dispatcher_remove_view(app->view_dispatcher, TempGraphView);

    view_free(app->main_view);
    view_free(app->temp_graph_view);

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
