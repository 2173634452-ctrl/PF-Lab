#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "common.h"

void compute_statistics(const WaterDataset *dataset);
double compute_mean(const double *values, size_t count);
double compute_stddev(const double *values, size_t count);
void generate_correlation_matrix(const WaterDataset *dataset, double matrix[6][6]);

#endif // ANALYSIS_H
