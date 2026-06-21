#include "graph.h"

void graph_draw_bar(
    Canvas* canvas,
    const int* data,
    int range,
    int count,
    int divisor,
    const char* title) {
    int min_raw = find_min(data, count);
    int max_raw = find_max(data, count);

    if(range <= 0) {
        range = max_raw - min_raw;
        if(range <= 0) {
            range = 1;
        }
    }

    char buf[16];

    canvas_set_font(canvas, FontPrimary);

    // Заголовок слева
    if(title) {
        canvas_draw_str(canvas, 60, 10, title);
    }

    snprintf(buf, sizeof(buf), "%d", max_raw / divisor);
    uint8_t w = canvas_string_width(canvas, buf);
    canvas_draw_str(canvas, 126 - w, 10, buf);

    snprintf(buf, sizeof(buf), "%d", min_raw / divisor);
    canvas_draw_str(canvas, 2, 10, buf);

    const int graph_top = 16;
    const int graph_bottom = 60;
    const int graph_height = graph_bottom - graph_top;

    const int bar_width = 5;
    const int step = (count == 24) ? 5 : 8;
    int x = (count == 24) ? 2 : 5;

    for(int i = 0; i < count; i++) {
        int height = ((data[i] - min_raw) * (graph_height - 2)) / range + 2;

        if(height > graph_height) {
            height = graph_height;
        }

        if(height < 2) {
            height = 2;
        }

        int y = graph_bottom - height;

        if(y < graph_top) {
            y = graph_top;
        }

        canvas_draw_box(canvas, x, y, bar_width, height);

        x += step;
    }
}
