#ifndef MODEL_H
#define MODEL_H

#include "common.h"

/* ── 参数类型枚举（与 preprocess.c / analysis.c 对齐）── */
typedef enum {
    PARAM_TEMP = 0,
    PARAM_SALINITY,
    PARAM_PH,
    PARAM_DO,
    PARAM_PRECIP,
    PARAM_AIR_TEMP
} ParamType;

/* ── 回归模型结构体 ── */
typedef struct {
    double a;           /* 斜率 */
    double b;           /* 截距 */
    double r_squared;   /* 决定系数 R² */
    double rmse;        /* 留出法均方根误差 */
    bool trained;       /* 是否已训练成功 */
    int feature_param;  /* 所用特征 (ParamType) */
} RegressionModel;

/* ── 4.1.1 训练单因素线性回归模型 ── */
void train_linear_regression(const WaterDataset *dataset, int feature_param,
                             RegressionModel *model);

/* ── 使用已训练模型预测 DO（依赖全局 g_model）── */
double predict_do_from_air_temp(double air_temp);

/* ── 4.1.2 模型评估（R² + 留出法 RMSE）── */
void evaluate_regression_model(const WaterDataset *dataset,
                               const RegressionModel *model);

/* ── 4.1.3 多因子探索（Temp/pH/Salinity → DO）── */
void explore_multi_factor(const WaterDataset *dataset);

/* ── 4.1 完整预测流程（训练→评估→多因子→报告）── */
void run_full_prediction_pipeline(const WaterDataset *dataset);

/* ── 获取全局回归模型（供 UI 查询状态）── */
const RegressionModel *get_regression_model(void);

#endif // MODEL_H
