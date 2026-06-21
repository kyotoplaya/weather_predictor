#pragma once

#include <stdio.h>
#include <string.h>

#include "../history/history.h"

void weather_prediction_get(PredictorHistory* history, char* text, int text_size);
