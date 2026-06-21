#pragma once

#include <gui/gui.h>
#include "../models/prediction_model.h"

void draw_temp_graph_callback(Canvas* canvas, void* model);

void draw_pressure_graph_callback(Canvas* canvas, void* model);

void draw_humidity_graph_callback(Canvas* canvas, void* model);
