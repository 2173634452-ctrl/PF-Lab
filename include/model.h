#ifndef MODEL_H
#define MODEL_H

#include "common.h"

void train_linear_regression(const WaterDataset *dataset);
double predict_do_from_air_temp(double air_temp);
void evaluate_regression_model(const WaterDataset *dataset);

#endif // MODEL_H
