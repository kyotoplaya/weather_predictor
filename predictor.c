#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "bmp180/bmp180.h"

#define TAG "Predictor"

typedef enum {
    PredictorViewMain,
} PredictorView;

typedef enum {
    PredictorEventRedraw = 0,
} PredictorEvent;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* main_view;
    FuriTimer* timer;
} PredictorApp;

typedef struct {
    int32_t temperature;
    int32_t pressure;
} PredictorModel;

static uint32_t predictor_exit_navigation_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void predictor_draw_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    canvas_clear(canvas);

    char buf[32];

    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
    canvas_draw_str(canvas, 5, 24, buf);

    canvas_set_font(canvas, FontPrimary);

    snprintf(buf, sizeof(buf), "T: %ld.%ld C", m->temperature / 10, labs(m->temperature % 10));
    canvas_draw_str(canvas, 5, 42, buf);

    snprintf(buf, sizeof(buf), "P: %ld Pa", m->pressure);
    canvas_draw_str(canvas, 5, 56, buf);
}

static void predictor_timer_callback(void* context) {
    PredictorApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, PredictorEventRedraw);
}

static void predictor_enter_callback(void* context) {
    PredictorApp* app = context;

    furi_assert(app->timer == NULL);

    app->timer = furi_timer_alloc(predictor_timer_callback, FuriTimerTypePeriodic, app);

    furi_timer_start(app->timer,
                     furi_ms_to_ticks(60000)); // 1 минута
}

static void predictor_exit_callback(void* context) {
    PredictorApp* app = context;

    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
}

static bool predictor_custom_event_callback(uint32_t event, void* context) {
    PredictorApp* app = context;

    switch(event) {
    case PredictorEventRedraw:
        with_view_model(
            app->main_view,
            PredictorModel * model,
            {
                model->temperature = get_temperature();
                model->pressure = get_pressure();
            },
            true);

        return true;

    default:
        return false;
    }
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

    view_set_context(app->main_view, app);

    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(PredictorModel));

    with_view_model(
        app->main_view,
        PredictorModel * model,
        {
            model->temperature = get_temperature();
            model->pressure = get_pressure();
        },
        false);

    view_set_draw_callback(app->main_view, predictor_draw_callback);

    view_set_enter_callback(app->main_view, predictor_enter_callback);

    view_set_exit_callback(app->main_view, predictor_exit_callback);

    view_set_previous_callback(app->main_view, predictor_exit_navigation_callback);

    view_set_custom_callback(app->main_view, predictor_custom_event_callback);

    view_dispatcher_add_view(app->view_dispatcher, PredictorViewMain, app->main_view);

    view_dispatcher_switch_to_view(app->view_dispatcher, PredictorViewMain);

    return app;
}

static void predictor_app_free(PredictorApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, PredictorViewMain);

    view_free(app->main_view);

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
