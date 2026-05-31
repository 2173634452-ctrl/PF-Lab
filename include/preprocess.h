#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "common.h"

/* ── 预处理统计信息结构体 ── */
typedef struct {
    size_t outlier_record_count;    /* 包含异常值的记录数 */
    size_t outlier_param_count;     /* 异常值参数总个数 */
    size_t deleted_record_count;    /* 因异常值≥3被整条删除的记录数 */
    size_t fixed_record_count;      /* 修复异常值的记录数（<3个异常，用均值逼近法填充） */
    size_t missing_value_count;     /* 处理的缺失值个数 */
    char outlier_time_start[32];    /* 异常数据时间跨度起始 */
    char outlier_time_end[32];      /* 异常数据时间跨度结束 */
} PreprocessStats;

/* ── 2.1 异常值检测与处理 ── */
void detect_and_handle_outliers(WaterDataset *dataset);

/* ── 2.2 缺失值处理——均值逼近法 ── */
void fill_missing_values(WaterDataset *dataset);

/* ── 2.3 移动平均滤波 ── */
void apply_moving_average(WaterDataset *dataset, int window_size);

/* 交互式滤波：让用户选择窗口大小，对比滤波前后效果 */
void interactive_moving_average(WaterDataset *dataset);

/* 获取预处理统计信息 */
const PreprocessStats *get_preprocess_stats(void);

/* 将预处理统计信息追加写入数据概览文件 */
void append_preprocess_overview(const WaterDataset *dataset);

#endif // PREPROCESS_H
