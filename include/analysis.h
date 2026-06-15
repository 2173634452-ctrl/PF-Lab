#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "common.h"

/* ── 3.1 基本统计量 ── */
void compute_statistics(const WaterDataset *dataset);

/* ── 3.2 分段统计 ── */
void hypoxia_warning(const WaterDataset *dataset);
void salinity_warning(const WaterDataset *dataset);

/* ── 3.3 相关性分析 ── */
void generate_correlation_matrix(const WaterDataset *dataset, double matrix[6][6]);

#endif // ANALYSIS_H
