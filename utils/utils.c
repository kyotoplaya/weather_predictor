#include "utils.h"

int find_min(const int* list, int size) {
    int32_t min = list[0];

    for(int i = 1; i < size; i++) {
        if(list[i] < min) min = list[i];
    }

    return (int)min;
}

int find_max(const int* list, int size) {
    int32_t max = list[0];

    for(int i = 1; i < size; i++) {
        if(list[i] > max) max = list[i];
    }

    return (int)max;
}

int get_avg(const int* list, int size) {
    int avg = 0;

    for(int i = 0; i < size; i++) {
        avg += (int)list[i];
    }

    return avg / size;
}
