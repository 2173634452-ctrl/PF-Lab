#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INITIAL_CAPACITY 1000
#define PAGE_SIZE 15

typedef struct {
    char timestamp[32];
    double temp;
    double salinity;
    double pH;
    double DO;
    double precipitation;
    double air_temp;
    bool valid;
} WaterRecord;

typedef struct {
    WaterRecord *records;
    size_t total_count;
    size_t valid_count;
    size_t capacity;
} WaterDataset;

#endif // COMMON_H
