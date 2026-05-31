#include "data_io.h"
#include <stdio.h>

WaterDataset *load_csv_data(const char *filename) {
    (void)filename;
    return NULL;
}

bool save_csv_data(const char *filename, const WaterDataset *dataset) {
    (void)filename;
    (void)dataset;
    return false;
}

bool save_binary_data(const char *filename, const WaterDataset *dataset) {
    (void)filename;
    (void)dataset;
    return false;
}

WaterDataset *load_binary_data(const char *filename) {
    (void)filename;
    return NULL;
}

void free_dataset(WaterDataset *dataset) {
    if (!dataset) return;
    free(dataset->records);
    free(dataset);
}
