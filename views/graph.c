#include "graph.h"

#include "../graph/graph.h"

void draw_temp_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    if(m->history->graph_interval == GraphInterval1H) {
        graph_draw_bar(canvas, m->history->temperature_1h, 50, 15, 10, "1h");
    } else {
        graph_draw_bar(canvas, m->history->temperature_day, 50, 24, 10, "24h");
    }
}

void draw_pressure_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    if(m->history->graph_interval == GraphInterval1H) {
        graph_draw_bar(canvas, m->history->pressure_1h, 20, 15, 1, "1h");
    } else {
        graph_draw_bar(canvas, m->history->pressure_day, 20, 24, 1, "24h");
    }
}

void draw_humidity_graph_callback(Canvas* canvas, void* model) {
    PredictorModel* m = model;

    canvas_clear(canvas);

    if(m->history->graph_interval == GraphInterval1H) {
        graph_draw_bar(canvas, m->history->humidity_1h, 100, 15, 1, "1h");
    } else {
        graph_draw_bar(canvas, m->history->humidity_day, 100, 24, 1, "24h");
    }
}
