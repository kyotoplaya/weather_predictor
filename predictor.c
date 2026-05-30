#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_rtc.h>

#include "bmp180/bmp180.h"

// Базовая структура состояния приложения
typedef struct {
    int32_t T;
    int32_t P;
} AppState;

// Функция отрисовки
static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* app = ctx;

    // Структура даты-времени
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    canvas_clear(canvas);

    char buf[32];

    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
    canvas_draw_str(canvas, 5, 24, buf);

    canvas_set_font(canvas, FontPrimary);

    snprintf(buf, sizeof(buf), "T: %ld.%ld C", app->T / 10, app->T % 10);
    canvas_draw_str(canvas, 5, 40, buf);

    snprintf(buf, sizeof(buf), "P: %ld Pa", app->P);
    canvas_draw_str(canvas, 5, 55, buf);
}

// Колбэк кнопок
static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

int32_t predictor_app(void* p) {
    UNUSED(p);

    AppState* app = malloc(sizeof(AppState));

    app->T = 0;
    app->P = 0;

    if(!bmp180_init()) {
        FURI_LOG_E("BMP", "init failed");
    }

    // Инициализации viewport
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();

    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);

    // API GUI и привязка к нему view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.key == InputKeyBack) {
                running = false;
            }
            // тут ты обновляешь датчики
            app->T = get_temperature();
            app->P = get_pressure();
        }

        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);

    view_port_free(view_port);
    furi_message_queue_free(queue);

    furi_record_close(RECORD_GUI);

    free(app);

    return 0;
}
