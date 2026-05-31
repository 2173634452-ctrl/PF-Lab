#ifndef DATA_IO_H
#define DATA_IO_H

#include "common.h"

/* ── 1.2 文件读取 ── */
WaterDataset *load_csv_data(const char *filename);
void write_data_overview(const WaterDataset *dataset);

/* ── 1.3 数据存储 ── */
bool save_csv_data(const char *filename, const WaterDataset *dataset);
bool save_binary_data(const char *filename, const WaterDataset *dataset);
WaterDataset *load_binary_data(const char *filename);
void compare_storage_performance(const WaterDataset *dataset);

/* ── 1.4 数据查询 ── */
void view_data_paginated(const WaterDataset *dataset);
void filter_data_by_range(const WaterDataset *dataset);
void sort_and_display_data(WaterDataset *dataset);

/* ── 1.5 数据修改 ── */
void modify_single_record(WaterDataset *dataset);

/* ── 1.6 数据删除 ── */
void delete_single_record(WaterDataset *dataset);
void batch_delete_records(WaterDataset *dataset);

/* ── 通用 ── */
void free_dataset(WaterDataset *dataset);

#endif // DATA_IO_H
