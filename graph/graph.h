#pragma once

#include <stdbool.h>
#include <gui/gui.h>

#include "../utils/utils.h"

void graph_draw_bar(
    Canvas* canvas,
    const int* data,
    int range,
    int count,
    int divisor,
    const char* title);
