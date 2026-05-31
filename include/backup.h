#ifndef BACKUP_H
#define BACKUP_H

#include "common.h"

bool backup_dataset(const char *directory, const WaterDataset *dataset);
WaterDataset *restore_dataset(const char *filepath);

#endif // BACKUP_H
