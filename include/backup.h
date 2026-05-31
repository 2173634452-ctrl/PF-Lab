#ifndef BACKUP_H
#define BACKUP_H

#include "common.h"

/* ── 1.7 数据备份与恢复 ── */
bool backup_dataset(const char *directory, const WaterDataset *dataset);
WaterDataset *restore_dataset(const char *filepath);
int list_backup_files(const char *directory, char filenames[][512], int max_count);
bool validate_backup_file(const char *filepath);

#endif // BACKUP_H
