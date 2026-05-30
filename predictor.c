#include <furi.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>

#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>

#include "bmp180/bmp180.h"

typedef enum {
    MainWidget,
    SecondWidget,
} Views;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Widget* main_widget;
    Widget* second_widget;
} App;

// Функция которая вызывается при нажатии кнопки назад
static bool back_callback(void* context) {
    furi_assert(context);
    App* app = context;

    // Кнопка «Назад» означает выход из приложения, что можно сделать, остановив ViewDispatcher.
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

// Колбэк для кнопок управления
static void app_button_callback(GuiButtonType button_type, InputType input_type, void* context) {
    furi_assert(context);
    App* app = context;

    // Сменить виджет только при коротком нажатии правой клавиши
    if(button_type == GuiButtonTypeRight && input_type == InputTypeShort) {
        // Меняем отображаемый виджет
        view_dispatcher_switch_to_view(app->view_dispatcher, SecondWidget);
    } else if(button_type == GuiButtonTypeLeft && input_type == InputTypeShort) {
        view_dispatcher_switch_to_view(app->view_dispatcher, MainWidget);
    }
}

// Создаем основной виджет
static void setup_main_widget(App* app) {
    // Запрашиваем температуру и делаем строку с ней
    int32_t T = get_temperature();
    char buf[64];
    snprintf(buf, sizeof(buf), "Temp: %ld.%ld C", T / 10, T % 10);

    app->main_widget = widget_alloc();
    widget_add_string_multiline_element(
        app->main_widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, buf);

    widget_add_button_element(
        app->main_widget, GuiButtonTypeRight, "Next view", app_button_callback, app);
}

// Создаем второй виджет
static void setup_second_widget(App* app) {
    // Запрашиваем давление и делаем строку с ним
    int32_t P = get_pressure();
    char buf[64];
    snprintf(buf, sizeof(buf), "Presure: %ld Pa", P);
    app->second_widget = widget_alloc();
    widget_add_string_multiline_element(
        app->second_widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, buf);

    widget_add_button_element(
        app->second_widget, GuiButtonTypeLeft, "Prev view", app_button_callback, app);
}

// Функция конструктора приложения
static App* app_alloc() {
    App* app = calloc(1, sizeof(App));

    // Доступ к API GUI
    Gui* gui = furi_record_open(RECORD_GUI);

    if(!bmp180_init()) {
        FURI_LOG_E("BMP180", "Cannot read calibration coefficients!");
    };

    setup_main_widget(app);
    setup_second_widget(app);

    // Создаем диспетчер
    app->view_dispatcher = view_dispatcher_alloc();

    //Даем знать GUI о диспетчере
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Дальше надо зарегистрировать представления (views). Они не будут показываться на экране сразу.
    // Каждое представление должно иметь свой собственный индекс для дальнейшей работы с ним.
    view_dispatcher_add_view(app->view_dispatcher, MainWidget, widget_get_view(app->main_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, SecondWidget, widget_get_view(app->second_widget));

    // Указываем колбэек при нажатии кнопки назад
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_callback);

    // Здесь указывается какой контекст передается колбэкам
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    return app;
}

static void app_free(App* app) {
    // Все представления должны быть дерегистрированы из диспетчера
    // до его удаления. Иначе краш
    view_dispatcher_remove_view(app->view_dispatcher, MainWidget);
    view_dispatcher_remove_view(app->view_dispatcher, SecondWidget);

    // Теперь безопасно можно удалить диспетчер
    view_dispatcher_free(app->view_dispatcher);

    // Удаляем все представления
    widget_free(app->main_widget);
    widget_free(app->second_widget);

    // Закрываем доступ к API GUI
    furi_record_close(RECORD_GUI);

    // Освобождаем память от приложения
    free(app);
}

static void app_run(App* app) {
    // Показать основной виджет на экране
    view_dispatcher_switch_to_view(app->view_dispatcher, MainWidget);

    // Эта функция будет блокирующей пока не сработает колбэк кнопки назад
    view_dispatcher_run(app->view_dispatcher);
}

int32_t predictor_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("TEST", "Predictor started");

    App* app = app_alloc();
    app_run(app);
    app_free(app);

    return 0;
}
