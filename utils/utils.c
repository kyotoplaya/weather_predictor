#include "utils.h"

int find_min(int* list) {
    int32_t min = list[0];

    for(int i = 1; i < LIST_SIZE; i++) {
        if(list[i] < min) min = list[i];
    }

    return (int)min;
}

int find_max(int* list) {
    int32_t max = list[0];

    for(int i = 1; i < LIST_SIZE; i++) {
        if(list[i] > max) max = list[i];
    }

    return (int)max;
}

int get_avg(int* list) {
    int avg = 0;

    for(int i = 0; i < LIST_SIZE; i++) {
        avg += (int)list[i];
    }

    return avg / LIST_SIZE;
}
