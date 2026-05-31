#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "common.h"

void analyze_missing_values(WaterDataset *dataset);
void fill_missing_values(WaterDataset *dataset);
void detect_and_handle_outliers(WaterDataset *dataset);
void apply_moving_average(WaterDataset *dataset, int window_size);

#endif // PREPROCESS_H
