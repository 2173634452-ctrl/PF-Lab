#ifndef DATA_IO_H
#define DATA_IO_H

#include "common.h"

WaterDataset *load_csv_data(const char *filename);
bool save_csv_data(const char *filename, const WaterDataset *dataset);
bool save_binary_data(const char *filename, const WaterDataset *dataset);
WaterDataset *load_binary_data(const char *filename);
void free_dataset(WaterDataset *dataset);

#endif // DATA_IO_H
