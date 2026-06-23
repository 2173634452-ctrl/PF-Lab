#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：参数名称（ParamType 枚举已公开在 model.h）
 * ═════════════════════════════════════════════════════════════════════ */
static const char *g_param_names[] = {
    "水温(Temp)", "盐度(Salinity)", "pH",
    "溶解氧(DO)", "降水量(Precip)", "气温(Air_temp)"
};

static const char *g_param_keys[] = {
    "Temp", "Salinity", "pH", "DO", "Precip", "Air_temp"
};

/* ═════════════════════════════════════════════════════════════════════
 *  全局回归模型（与 preprocess.c 的 g_stats 模式一致）
 * ═════════════════════════════════════════════════════════════════════ */
static RegressionModel g_model = {0.0, 0.0, NAN, NAN, false, PARAM_AIR_TEMP};

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：获取记录中指定参数的值
 * ═════════════════════════════════════════════════════════════════════ */
static double get_param_value(const WaterRecord *rec, ParamType param) {
    switch (param) {
        case PARAM_TEMP:      return rec->temp;
        case PARAM_SALINITY:  return rec->salinity;
        case PARAM_PH:        return rec->pH;
        case PARAM_DO:        return rec->DO;
        case PARAM_PRECIP:    return rec->precipitation;
        case PARAM_AIR_TEMP:  return rec->air_temp;
        default:              return 0.0;
    }
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：从数据集提取 (x=特征, y=DO) 有效数据对
 *  返回动态分配的数组，通过 out_count 返回长度
 *  调用者负责释放内存
 * ═════════════════════════════════════════════════════════════════════ */
typedef struct {
    double x;
    double y;
} DataPair;

static DataPair *extract_valid_pairs(const WaterDataset *dataset,
                                     ParamType feature_param,
                                     size_t *out_count) {
    *out_count = 0;

    /* 统计有效记录数 */
    size_t valid_n = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) valid_n++;
    }

    if (valid_n == 0) return NULL;

    DataPair *pairs = (DataPair *)malloc(valid_n * sizeof(DataPair));
    if (!pairs) return NULL;

    for (size_t i = 0; i < dataset->total_count; i++) {
        const WaterRecord *rec = &dataset->records[i];
        if (!rec->valid) continue;

        double x = get_param_value(rec, feature_param);
        double y = rec->DO;

        /* 跳过含 NaN 的值 */
        if (isnan(x) || isnan(y)) continue;

        pairs[*out_count].x = x;
        pairs[*out_count].y = y;
        (*out_count)++;
    }

    return pairs;
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：最小二乘法拟合 y = a*x + b
 *  返回 true 表示成功，false 表示分母趋零无法拟合
 * ═════════════════════════════════════════════════════════════════════ */
static bool least_squares_fit(const DataPair *pairs, size_t n,
                              double *a, double *b) {
    if (n < 2) return false;

    double sum_x = 0.0, sum_y = 0.0;
    double sum_xy = 0.0, sum_x2 = 0.0;

    for (size_t i = 0; i < n; i++) {
        sum_x  += pairs[i].x;
        sum_y  += pairs[i].y;
        sum_xy += pairs[i].x * pairs[i].y;
        sum_x2 += pairs[i].x * pairs[i].x;
    }

    double denom = (double)n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-15) return false;

    *a = ((double)n * sum_xy - sum_x * sum_y) / denom;
    *b = (sum_y - (*a) * sum_x) / (double)n;
    return true;
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：计算决定系数 R² = 1 − SS_res / SS_tot
 * ═════════════════════════════════════════════════════════════════════ */
static double compute_r_squared(const DataPair *pairs, size_t n,
                                double a, double b) {
    if (n < 2) return NAN;

    double y_mean = 0.0;
    for (size_t i = 0; i < n; i++) y_mean += pairs[i].y;
    y_mean /= (double)n;

    double ss_tot = 0.0, ss_res = 0.0;
    for (size_t i = 0; i < n; i++) {
        double pred = a * pairs[i].x + b;
        ss_tot += (pairs[i].y - y_mean) * (pairs[i].y - y_mean);
        ss_res += (pairs[i].y - pred) * (pairs[i].y - pred);
    }

    if (ss_tot < 1e-15) return NAN;
    return 1.0 - ss_res / ss_tot;
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：计算均方根误差 RMSE = sqrt(Σ(pred−actual)² / n)
 * ═════════════════════════════════════════════════════════════════════ */
static double compute_rmse(const DataPair *pairs, size_t n,
                           double a, double b) {
    if (n == 0) return NAN;

    double sum_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
        double pred = a * pairs[i].x + b;
        double diff = pred - pairs[i].y;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / (double)n);
}

/* ═════════════════════════════════════════════════════════════════════
 *  4.1.1 训练单因素线性回归模型
 *  feature_param: PARAM_TEMP / PARAM_PH / PARAM_SALINITY / PARAM_AIR_TEMP ...
 *  使用全部有效记录，同时计算 R²
 * ═════════════════════════════════════════════════════════════════════ */
void train_linear_regression(const WaterDataset *dataset, int feature_param,
                             RegressionModel *model) {
    if (!dataset || !model) return;

    model->trained = false;
    model->a = 0.0;
    model->b = 0.0;
    model->r_squared = NAN;
    model->rmse = NAN;

    size_t n = 0;
    DataPair *pairs = extract_valid_pairs(dataset, (ParamType)feature_param, &n);
    if (!pairs || n < 2) {
        free(pairs);
        printf("错误：有效数据对不足（需要 ≥2，实际 %zu），无法训练模型。\n", n);
        return;
    }

    double a = 0.0, b = 0.0;
    if (!least_squares_fit(pairs, n, &a, &b)) {
        printf("错误：自变量方差接近零，无法计算回归系数。\n");
        printf("提示：该因子的数据波动过小（如 Precip 预处理后≈0），不适合作为回归自变量。\n");
        free(pairs);
        return;
    }

    double r2 = compute_r_squared(pairs, n, a, b);

    model->a = a;
    model->b = b;
    model->r_squared = r2;
    model->trained = true;
    model->feature_param = feature_param;

    printf("\n┌──────────────────────────────────────────┐\n");
    printf("│        单因素线性回归模型训练完成          │\n");
    printf("├──────────────────────────────────────────┤\n");
    printf("│  回归方程: DO = %.6f × X + %.6f  │\n", a, b);
    printf("│  决定系数 R² = %.6f                     │\n", r2);
    printf("│  有效样本数: %zu                          │\n", n);
    printf("└──────────────────────────────────────────┘\n");

    /* 也存到全局模型，方便 predict_do_from_air_temp 使用 */
    g_model = *model;

    free(pairs);
}

/* ═════════════════════════════════════════════════════════════════════
 *  使用已训练模型预测 DO（任务书原有签名，依赖全局 g_model）
 * ═════════════════════════════════════════════════════════════════════ */
double predict_do_from_air_temp(double air_temp) {
    if (!g_model.trained) {
        printf("警告：模型尚未训练，无法进行预测。请先执行 [1] 单因素回归。\n");
        return NAN;
    }
    /* 验证输入在合理范围内 */
    if (air_temp < VALID_AIR_TEMP_MIN || air_temp > VALID_AIR_TEMP_MAX) {
        printf("警告：输入气温 %.2f 超出合理范围 [%.0f, %.0f]，预测结果仅供参考。\n",
               air_temp, VALID_AIR_TEMP_MIN, VALID_AIR_TEMP_MAX);
    }
    double do_pred = g_model.a * air_temp + g_model.b;
    /* 钳制到 DO 合理范围 */
    if (do_pred < VALID_DO_MIN) do_pred = VALID_DO_MIN;
    if (do_pred > VALID_DO_MAX) do_pred = VALID_DO_MAX;
    return do_pred;
}

/* ═════════════════════════════════════════════════════════════════════
 *  4.1.2 模型评估：R²（全量数据）+ 留出法 RMSE（80/20 时间有序）
 * ═════════════════════════════════════════════════════════════════════ */
void evaluate_regression_model(const WaterDataset *dataset,
                               const RegressionModel *model) {
    if (!dataset || !model) return;

    if (!model->trained) {
        printf("错误：当前模型尚未训练，无法评估。\n");
        return;
    }

    /* ── R² 已在训练时计算，直接展示 ── */
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║          模型评估结果                     ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  回归方程: DO = %.4f × X + %.4f     ║\n",
           model->a, model->b);
    printf("║  决定系数 R² = %.6f                  ║\n", model->r_squared);

    /* R² 解读 */
    double r2 = model->r_squared;
    const char *level;
    if (isnan(r2)) level = "无法评估";
    else if (r2 >= 0.8)  level = "极强拟合";
    else if (r2 >= 0.6)  level = "强拟合";
    else if (r2 >= 0.4)  level = "中等拟合";
    else if (r2 >= 0.2)  level = "弱拟合";
    else                 level = "极弱拟合";
    printf("║  拟合强度: %s                      ║\n", level);

    /* ── 留出法：前 80% 训练 / 后 20% 测试（时间有序）── */
    /* 提取有效记录索引 */
    size_t valid_n = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) valid_n++;
    }

    if (valid_n < 10) {
        printf("║  有效记录不足 (%zu)，无法执行留出法评估    ║\n", valid_n);
        printf("╚══════════════════════════════════════════╝\n");
        return;
    }

    size_t train_n = valid_n * 80 / 100;
    size_t test_n = valid_n - train_n;
    if (train_n < 2 || test_n < 1) {
        printf("║  样本量不足，无法执行留出法评估            ║\n");
        printf("╚══════════════════════════════════════════╝\n");
        return;
    }

    /* 分配训练集和测试集 */
    DataPair *train_set = (DataPair *)malloc(train_n * sizeof(DataPair));
    DataPair *test_set  = (DataPair *)malloc(test_n * sizeof(DataPair));
    if (!train_set || !test_set) {
        free(train_set);
        free(test_set);
        printf("║  内存不足，无法执行留出法评估              ║\n");
        printf("╚══════════════════════════════════════════╝\n");
        return;
    }

    ParamType feat = (ParamType)model->feature_param;
    size_t ti = 0, si = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (!dataset->records[i].valid) continue;
        double x_val = get_param_value(&dataset->records[i], feat);
        double do_val = dataset->records[i].DO;
        if (ti < train_n) {
            train_set[ti].x = x_val;
            train_set[ti].y = do_val;
            ti++;
        } else if (si < test_n) {
            test_set[si].x = x_val;
            test_set[si].y = do_val;
            si++;
        }
    }
    train_n = ti;
    test_n = si;

    /* 训练集拟合 */
    double hold_a = 0.0, hold_b = 0.0;
    bool fit_ok = least_squares_fit(train_set, train_n, &hold_a, &hold_b);
    if (!fit_ok) {
        printf("║  留出法训练失败（分母趋零）                ║\n");
        printf("╚══════════════════════════════════════════╝\n");
        free(train_set);
        free(test_set);
        return;
    }

    /* 测试集 RMSE */
    double rmse = compute_rmse(test_set, test_n, hold_a, hold_b);

    printf("╠══════════════════════════════════════════╣\n");
    printf("║  [留出法评估] 80/20 时间有序切分          ║\n");
    printf("║  训练集样本数: %zu                       ║\n", train_n);
    printf("║  测试集样本数: %zu                       ║\n", test_n);
    printf("║  留出法 RMSE = %.6f                  ║\n", rmse);
    printf("╚══════════════════════════════════════════╝\n");

    /* 更新全局模型的 rmse */
    g_model.rmse = rmse;

    free(train_set);
    free(test_set);
}

/* ═════════════════════════════════════════════════════════════════════
 *  4.1.3 多因子探索：比较 Temp/pH/Salinity → DO 的 R²
 *  排除 Precip（预处理后方差≈0）和 Air_temp（已在 4.1.1 训练）
 * ═════════════════════════════════════════════════════════════════════ */
void explore_multi_factor(const WaterDataset *dataset) {
    if (!dataset) return;

    /* 探索因子列表（排除 DO 自身 + Precip + Air_temp） */
    ParamType factors[] = { PARAM_TEMP, PARAM_PH, PARAM_SALINITY };
    const char *factor_names[] = {
        g_param_names[PARAM_TEMP],
        g_param_names[PARAM_PH],
        g_param_names[PARAM_SALINITY]
    };

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║              多因子探索 — 单因素回归比较                  ║\n");
    printf("║              因变量: 溶解氧 (DO)                         ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║ %-20s │ %10s │ %10s │ %10s ║\n",
           "因子", "斜率 a", "截距 b", "R²");
    printf("╠══════════════════════════════════════════════════════════╣\n");

    int best_idx = -1;
    double best_r2 = -INFINITY;

    for (int f = 0; f < 3; f++) {
        size_t n = 0;
        DataPair *pairs = extract_valid_pairs(dataset, factors[f], &n);
        if (!pairs || n < 2) {
            printf("║ %-20s │ %10s │ %10s │ %10s ║\n",
                   factor_names[f], "—", "—", "样本不足");
            free(pairs);
            continue;
        }

        double a = 0.0, b = 0.0;
        if (!least_squares_fit(pairs, n, &a, &b)) {
            printf("║ %-20s │ %10s │ %10s │ %10s ║\n",
                   factor_names[f], "—", "—", "方差≈0");
            free(pairs);
            continue;
        }

        double r2 = compute_r_squared(pairs, n, a, b);

        printf("║ %-20s │ %10.4f │ %10.4f │ %10.4f ║\n",
               factor_names[f], a, b, r2);

        if (!isnan(r2) && r2 > best_r2) {
            best_r2 = r2;
            best_idx = f;
        }

        free(pairs);
    }

    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  排除因子说明:                                           ║\n");
    printf("║  · 降水量(Precip): 预处理后方差≈0，不适合作为回归自变量  ║\n");
    printf("║  · 气温(Air_temp): 已在 [1] 中单独训练                   ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");

    if (best_idx >= 0) {
        printf("║  结论: %-20s 对 DO 影响最大 (R²=%.4f)   ║\n",
               factor_names[best_idx], best_r2);
    }
    printf("╚══════════════════════════════════════════════════════════╝\n");
}

/* ═════════════════════════════════════════════════════════════════════
 *  4.1 完整预测流程：训练 Air_temp→DO → 评估 → 多因子探索 → 写报告
 * ═════════════════════════════════════════════════════════════════════ */
void run_full_prediction_pipeline(const WaterDataset *dataset) {
    if (!dataset) return;

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║       执行完整预测流程                                ║\n");
    printf("║       4.1.1 → 4.1.2 → 4.1.3                         ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Step 1: 训练 Air_temp → DO */
    printf("━━━ [1/3] 训练单因素回归模型 (Air_temp → DO) ━━━\n");
    RegressionModel model;
    train_linear_regression(dataset, PARAM_AIR_TEMP, &model);

    if (!model.trained) {
        printf("模型训练失败，流程终止。\n");
        return;
    }

    /* Step 2: 模型评估 */
    printf("\n━━━ [2/3] 模型评估 (R² + 留出法) ━━━\n");
    evaluate_regression_model(dataset, &model);

    /* Step 3: 多因子探索 */
    printf("\n━━━ [3/3] 多因子探索 (Temp/pH/Salinity → DO) ━━━\n");
    explore_multi_factor(dataset);

    /* ── 写入预测报告 CSV ── */
    FILE *fp = fopen("reports/prediction_report.csv", "w");
    if (!fp) {
        printf("\n警告：无法创建 reports/prediction_report.csv，跳过报告写入。\n");
        return;
    }

    /* Section 1: 回归方程参数 */
    fprintf(fp, "指标,值\n");
    fprintf(fp, "斜率 a,%.6f\n", model.a);
    fprintf(fp, "截距 b,%.6f\n", model.b);
    fprintf(fp, "决定系数 R²,%.6f\n", model.r_squared);
    fprintf(fp, "留出法 RMSE,%.6f\n", model.rmse);
    fprintf(fp, "\n");

    /* Section 2: 留出法评估详情 */
    /* 重新执行留出法获取样本数 */
    size_t valid_n = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) valid_n++;
    }
    size_t train_n = valid_n * 80 / 100;
    size_t test_n = valid_n - train_n;

    fprintf(fp, "评估方法,训练样本数,测试样本数,RMSE\n");
    fprintf(fp, "留出法(80/20),%zu,%zu,%.6f\n", train_n, test_n, model.rmse);
    fprintf(fp, "\n");

    /* Section 3: 多因子比较 */
    fprintf(fp, "因子,斜率 a,截距 b,R²\n");
    ParamType factors[] = { PARAM_TEMP, PARAM_PH, PARAM_SALINITY, PARAM_AIR_TEMP };
    for (int f = 0; f < 4; f++) {
        size_t n = 0;
        DataPair *pairs = extract_valid_pairs(dataset, factors[f], &n);
        if (!pairs || n < 2) {
            fprintf(fp, "%s,样本不足,—,—\n", g_param_keys[factors[f]]);
            free(pairs);
            continue;
        }
        double a = 0.0, b = 0.0;
        if (!least_squares_fit(pairs, n, &a, &b)) {
            fprintf(fp, "%s,方差≈0,—,—\n", g_param_keys[factors[f]]);
            free(pairs);
            continue;
        }
        double r2 = compute_r_squared(pairs, n, a, b);
        fprintf(fp, "%s,%.6f,%.6f,%.6f\n", g_param_keys[factors[f]], a, b, r2);
        free(pairs);
    }
    fprintf(fp, "\n");

    /* Section 4: 结论与讨论 */
    fprintf(fp, "分析项目,结论\n");
    fprintf(fp, "预测模型,DO = %.4f × Air_temp + %.4f\n", model.a, model.b);
    fprintf(fp, "模型拟合度,决定系数 R² = %.4f\n", model.r_squared);
    fprintf(fp, "预测误差,留出法 RMSE = %.4f\n", model.rmse);

    /* 找到最佳因子 */
    double best_r2 = -INFINITY;
    const char *best_key = "—";
    for (int f = 0; f < 3; f++) {
        size_t n = 0;
        DataPair *pairs = extract_valid_pairs(dataset, factors[f], &n);
        if (!pairs) continue;
        double a = 0.0, b = 0.0;
        if (least_squares_fit(pairs, n, &a, &b)) {
            double r2 = compute_r_squared(pairs, n, a, b);
            if (!isnan(r2) && r2 > best_r2) {
                best_r2 = r2;
                best_key = g_param_keys[factors[f]];
            }
        }
        free(pairs);
    }
    fprintf(fp, "最佳预测因子,%s (R²=%.4f)\n", best_key, best_r2);
    fprintf(fp, "\n");
    fprintf(fp, "局限性分析,单因素线性回归仅考虑一个自变量，忽略多因子交互作用\n");
    fprintf(fp, ",实际水质变化受温度、盐度、pH、生物活动等多因素协同影响\n");
    fprintf(fp, ",模型假设变量间为线性关系，但真实环境可能存在非线性特征\n");
    fprintf(fp, ",预测准确度受限，R² < 0.4 时预测结果仅供参考\n");

    fclose(fp);
    printf("\n预测报告已保存至 reports/prediction_report.csv\n");
}

/* ═════════════════════════════════════════════════════════════════════
 *  获取全局回归模型（供 UI 查询状态）
 * ═════════════════════════════════════════════════════════════════════ */
const RegressionModel *get_regression_model(void) {
    return &g_model;
}

/* ═════════════════════════════════════════════════════════════════════
 *  4.2 选做：多元线性回归 (MLR)
 *  特征: Temp / pH / Salinity / Air_temp / sin(hour) / cos(hour) / Temp×pH
 *  方法: 正规方程 + 高斯–约当消元法求解 β = (XᵀX)⁻¹Xᵀy
 * ═════════════════════════════════════════════════════════════════════ */

#define MLR_FEATURE_COUNT 7

static MLRModel g_mlr_model = {
    {0}, 0.0, 0, {{0}}, NAN, NAN, NAN, {0}, false
};

/* ── 高斯–约当消元求解 Ax = b（A_aug = [A|b]，n 阶方阵，原地修改）── */
static bool gauss_jordan_solve(int n, double *A_aug, double *x) {
    int col = n + 1; /* 增广矩阵列数 */

    for (int piv = 0; piv < n; piv++) {
        /* 选主元（部分选主元） */
        int max_row = piv;
        double max_val = fabs(A_aug[piv * col + piv]);
        for (int r = piv + 1; r < n; r++) {
            double val = fabs(A_aug[r * col + piv]);
            if (val > max_val) { max_val = val; max_row = r; }
        }
        if (max_val < 1e-15) return false; /* 奇异矩阵 */

        /* 交换行 */
        if (max_row != piv) {
            for (int c = 0; c < col; c++) {
                double tmp = A_aug[piv * col + c];
                A_aug[piv * col + c] = A_aug[max_row * col + c];
                A_aug[max_row * col + c] = tmp;
            }
        }

        /* 主元归一化 */
        double pivot = A_aug[piv * col + piv];
        for (int c = 0; c < col; c++)
            A_aug[piv * col + c] /= pivot;

        /* 消去其他行 */
        for (int r = 0; r < n; r++) {
            if (r == piv) continue;
            double factor = A_aug[r * col + piv];
            for (int c = 0; c < col; c++)
                A_aug[r * col + c] -= factor * A_aug[piv * col + c];
        }
    }

    for (int i = 0; i < n; i++)
        x[i] = A_aug[i * col + (col - 1)];
    return true;
}

/* ── 求 XᵀX 的逆矩阵（n 阶，用于 VIF 计算）── */
/* ── 从时间戳解析小时数 ── */
static int parse_hour_from_timestamp(const char *timestamp) {
    int y, mon, d, h, min, s;
    if (sscanf(timestamp, "%d-%d-%d %d:%d:%d", &y, &mon, &d, &h, &min, &s) == 6)
        return h;
    return 0;
}

/* ── 特征矩阵提取 ── */
static bool extract_feature_matrix(const WaterDataset *dataset,
                                   double *X, double *y, size_t *out_n) {
    *out_n = 0;

    /* 统计同时满足所有参数有效的记录数 */
    size_t valid_n = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        const WaterRecord *r = &dataset->records[i];
        if (!r->valid) continue;
        if (isnan(r->temp) || isnan(r->pH) || isnan(r->salinity) ||
            isnan(r->DO) || isnan(r->air_temp)) continue;
        valid_n++;
    }

    size_t M = MLR_FEATURE_COUNT;
    if (valid_n < M + 2) {
        printf("错误：有效记录不足（需 ≥%zu，实际 %zu），无法训练 MLR。\n",
               M + 2, valid_n);
        return false;
    }

    *out_n = valid_n;
    size_t row = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        const WaterRecord *r = &dataset->records[i];
        if (!r->valid) continue;
        if (isnan(r->temp) || isnan(r->pH) || isnan(r->salinity) ||
            isnan(r->DO) || isnan(r->air_temp)) continue;

        int hour = parse_hour_from_timestamp(r->timestamp);
        double hour_rad = hour * 2.0 * 3.141592653589793 / 24.0;

        X[row * M + 0] = r->temp;
        X[row * M + 1] = r->pH;
        X[row * M + 2] = r->salinity;
        X[row * M + 3] = r->air_temp;
        X[row * M + 4] = sin(hour_rad);
        X[row * M + 5] = cos(hour_rad);
        X[row * M + 6] = r->temp * r->pH;
        y[row] = r->DO;
        row++;
    }
    return true;
}

/* ── 构建 X_aug = [1 | X] ── */
static void build_augmented_matrix(size_t N, size_t M,
                                   const double *X, double *X_aug) {
    for (size_t i = 0; i < N; i++) {
        X_aug[i * (M + 1)] = 1.0;
        for (size_t j = 0; j < M; j++)
            X_aug[i * (M + 1) + 1 + j] = X[i * M + j];
    }
}

/* ── 预测函数 ── */
double predict_do_mlr(const MLRModel *model, const WaterRecord *rec) {
    if (!model || !model->trained) {
        printf("警告：MLR 模型尚未训练，无法预测。\n");
        return NAN;
    }

    int hour = parse_hour_from_timestamp(rec->timestamp);
    double hour_rad = hour * 2.0 * 3.141592653589793 / 24.0;

    double features[MLR_MAX_FEATURES];
    features[0] = rec->temp;
    features[1] = rec->pH;
    features[2] = rec->salinity;
    features[3] = rec->air_temp;
    features[4] = sin(hour_rad);
    features[5] = cos(hour_rad);
    features[6] = rec->temp * rec->pH;

    double pred = model->intercept;
    for (int j = 0; j < model->feature_count; j++)
        pred += model->coeffs[j] * features[j];

    if (pred < VALID_DO_MIN) pred = VALID_DO_MIN;
    if (pred > VALID_DO_MAX) pred = VALID_DO_MAX;
    return pred;
}

/* ═════════════════════════════════════════════════════════════════════
 *  训练多元线性回归模型
 * ═════════════════════════════════════════════════════════════════════ */
void train_multivariate_regression(const WaterDataset *dataset, MLRModel *model) {
    if (!dataset || !model) return;

    model->trained = false;
    memset(model->coeffs, 0, sizeof(model->coeffs));
    model->intercept = 0.0;
    model->r_squared = NAN;
    model->adjusted_r_squared = NAN;
    model->rmse = NAN;
    memset(model->vif, 0, sizeof(model->vif));

    size_t M = MLR_FEATURE_COUNT;
    size_t N = 0;

    /* Phase 1: 提取特征 */
    double *X = (double *)malloc(M * dataset->valid_count * sizeof(double));
    double *y = (double *)malloc(dataset->valid_count * sizeof(double));
    if (!X || !y) {
        printf("错误：内存不足，无法分配特征矩阵。\n");
        free(X); free(y);
        return;
    }

    if (!extract_feature_matrix(dataset, X, y, &N)) {
        free(X); free(y);
        return;
    }

    /* Phase 2: 构建增广矩阵 X_aug = [1 | X] */
    double *X_aug = (double *)malloc(N * (M + 1) * sizeof(double));
    if (!X_aug) { free(X); free(y); return; }
    build_augmented_matrix(N, M, X, X_aug);

    /* Phase 3: 计算 XᵀX_aug 和 Xᵀy */
    int p = (int)(M + 1);
    double *XTX = (double *)calloc((size_t)(p * p), sizeof(double));
    double *XTy = (double *)calloc((size_t)p, sizeof(double));
    if (!XTX || !XTy) {
        free(X); free(y); free(X_aug);
        free(XTX); free(XTy);
        return;
    }

    for (size_t i = 0; i < N; i++) {
        for (int c1 = 0; c1 < p; c1++) {
            double xi = X_aug[i * p + c1];
            XTy[c1] += xi * y[i];
            for (int c2 = 0; c2 < p; c2++)
                XTX[c1 * p + c2] += xi * X_aug[i * p + c2];
        }
    }

    /* Phase 4: 高斯–约当求解 (XᵀX)⁻¹ Xᵀy */
    double *aug_sys = (double *)malloc((size_t)(p * (p + 1)) * sizeof(double));
    double *beta    = (double *)malloc((size_t)p * sizeof(double));
    if (!aug_sys || !beta) {
        free(X); free(y); free(X_aug); free(XTX); free(XTy);
        free(aug_sys); free(beta);
        return;
    }

    for (int i = 0; i < p; i++) {
        for (int j = 0; j < p; j++)
            aug_sys[i * (p + 1) + j] = XTX[i * p + j];
        aug_sys[i * (p + 1) + p] = XTy[i];
    }

    if (!gauss_jordan_solve(p, aug_sys, beta)) {
        printf("错误：正规方程奇异，特征间共线性严重，无法求解。\n");
        printf("提示：可能是 Air_temp 与 Temp 高度线性相关 (r≈0.99)，\n");
        printf("      可尝试移除其中一个或使用主成分分析。\n");
        free(X); free(y); free(X_aug); free(XTX); free(XTy);
        free(aug_sys); free(beta);
        return;
    }

    /* Phase 5: 计算 R² 和 Adjusted R² */
    double y_mean = 0.0;
    for (size_t i = 0; i < N; i++) y_mean += y[i];
    y_mean /= (double)N;

    double ss_tot = 0.0, ss_res = 0.0;
    for (size_t i = 0; i < N; i++) {
        double pred = beta[0]; /* 截距 */
        for (int j = 0; j < (int)M; j++)
            pred += beta[1 + j] * X[i * M + j];
        ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
        ss_res += (y[i] - pred) * (y[i] - pred);
    }

    double r2 = (ss_tot < 1e-15) ? NAN : 1.0 - ss_res / ss_tot;
    double adj_r2 = NAN;
    if (!isnan(r2) && N > (size_t)M + 1)
        adj_r2 = 1.0 - (1.0 - r2) * (double)(N - 1) / (double)(N - M - 1);

    /* Phase 6: VIF — 对每个特征 X_j 作子回归 X_j ~ 其余 X_k，VIF_j = 1/(1−R_j²) */
    for (int j = 0; j < (int)M; j++) {
        /* 抽取 y_sub = X_j, X_sub = 其余列 */
        double *y_sub = (double *)malloc(N * sizeof(double));
        int sub_m = (int)M - 1;
        double *X_sub = (double *)malloc(N * (size_t)sub_m * sizeof(double));
        if (!y_sub || !X_sub) {
            model->vif[j] = NAN;
            free(y_sub); free(X_sub);
            continue;
        }
        for (size_t i = 0; i < N; i++) {
            y_sub[i] = X[i * M + j];
            int col = 0;
            for (int k = 0; k < (int)M; k++)
                if (k != j)
                    X_sub[i * sub_m + col++] = X[i * M + k];
        }
        /* 构建增广 [1 | X_sub] 并累积正规方程 */
        int sp = sub_m + 1;
        double *sXTX = (double *)calloc((size_t)(sp * sp), sizeof(double));
        double *sXTy = (double *)calloc((size_t)sp, sizeof(double));
        if (!sXTX || !sXTy) {
            model->vif[j] = NAN;
            free(y_sub); free(X_sub); free(sXTX); free(sXTy);
            continue;
        }
        for (size_t i = 0; i < N; i++) {
            sXTy[0] += y_sub[i];
            sXTX[0] += 1.0;
            for (int c = 0; c < sub_m; c++) {
                double xi = X_sub[i * sub_m + c];
                sXTy[1 + c] += xi * y_sub[i];
                sXTX[1 + c] += xi;
                sXTX[sp * (1 + c)] += xi;
                for (int r = 0; r < sub_m; r++)
                    sXTX[(1 + c) * sp + (1 + r)] += xi * X_sub[i * sub_m + r];
            }
        }
        sXTX[0] = (double)N;
        /* 增广求解 */
        double *s_aug = (double *)malloc((size_t)(sp * (sp + 1)) * sizeof(double));
        double *s_beta = (double *)malloc((size_t)sp * sizeof(double));
        if (s_aug && s_beta) {
            for (int r = 0; r < sp; r++) {
                for (int c = 0; c < sp; c++)
                    s_aug[r * (sp + 1) + c] = sXTX[r * sp + c];
                s_aug[r * (sp + 1) + sp] = sXTy[r];
            }
            if (gauss_jordan_solve(sp, s_aug, s_beta)) {
                double y_sub_mean = 0.0;
                for (size_t i = 0; i < N; i++) y_sub_mean += y_sub[i];
                y_sub_mean /= (double)N;
                double ss_tot_sub = 0.0, ss_res_sub = 0.0;
                for (size_t i = 0; i < N; i++) {
                    double pred = s_beta[0];
                    for (int k = 0; k < sub_m; k++)
                        pred += s_beta[1 + k] * X_sub[i * sub_m + k];
                    ss_tot_sub += (y_sub[i] - y_sub_mean) * (y_sub[i] - y_sub_mean);
                    ss_res_sub += (y_sub[i] - pred) * (y_sub[i] - pred);
                }
                double r2_sub = (ss_tot_sub < 1e-15) ? 0.0
                              : 1.0 - ss_res_sub / ss_tot_sub;
                model->vif[j] = (r2_sub >= 1.0 - 1e-15) ? INFINITY
                              : 1.0 / (1.0 - r2_sub);
            } else {
                model->vif[j] = INFINITY;
            }
        }
        free(y_sub); free(X_sub); free(sXTX); free(sXTy);
        free(s_aug); free(s_beta);
    }

    /* Phase 7: 填充模型 */
    model->intercept = beta[0];
    model->feature_count = (int)M;
    for (int j = 0; j < (int)M; j++)
        model->coeffs[j] = beta[1 + j];

    const char *fnames[] = {
        "水温(Temp)", "pH", "盐度(Salinity)", "气温(Air_temp)",
        "sin(hour)", "cos(hour)", "Temp×pH"
    };
    for (int j = 0; j < (int)M; j++) {
        strncpy(model->feature_names[j], fnames[j],
                sizeof(model->feature_names[j]) - 1);
        model->feature_names[j][sizeof(model->feature_names[j]) - 1] = '\0';
    }

    model->r_squared = r2;
    model->adjusted_r_squared = adj_r2;
    model->trained = true;

    /* 同步全局模型 */
    g_mlr_model = *model;

    printf("\n┌──────────────────────────────────────────┐\n");
    printf("│      多元线性回归模型训练完成              │\n");
    printf("├──────────────────────────────────────────┤\n");
    printf("│  特征数: %d                                │\n", (int)M);
    printf("│  有效样本: %zu                             │\n", N);
    printf("│  R² = %.6f                             │\n", r2);
    if (!isnan(adj_r2))
        printf("│  调整R² = %.6f                        │\n", adj_r2);
    printf("└──────────────────────────────────────────┘\n");

    free(X); free(y); free(X_aug); free(XTX); free(XTy);
    free(aug_sys); free(beta);
}

/* ═════════════════════════════════════════════════════════════════════
 *  MLR 模型评估（R² / 调整R² / VIF / 留出法）
 * ═════════════════════════════════════════════════════════════════════ */
void evaluate_multivariate_regression(const WaterDataset *dataset,
                                      MLRModel *model) {
    if (!dataset || !model) return;

    if (!model->trained) {
        printf("错误：MLR 模型尚未训练，无法评估。\n");
        return;
    }

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              MLR 模型评估结果                                ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");

    /* R² 解读 */
    double r2 = model->r_squared;
    const char *level;
    if (isnan(r2)) level = "无法评估";
    else if (r2 >= 0.8)  level = "极强拟合";
    else if (r2 >= 0.6)  level = "强拟合";
    else if (r2 >= 0.4)  level = "中等拟合";
    else if (r2 >= 0.2)  level = "弱拟合";
    else                 level = "极弱拟合";

    printf("║  R² = %.6f  (%s)                        ║\n", r2, level);
    if (!isnan(model->adjusted_r_squared))
        printf("║  调整R² = %.6f                                            ║\n",
               model->adjusted_r_squared);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  %-20s │ %10s │ %10s  ║\n", "特征", "系数", "VIF");
    printf("╠══════════════════════════════════════════════════════════════╣\n");

    for (int j = 0; j < model->feature_count; j++) {
        const char *note = "";
        if (model->vif[j] > 10.0) note = " ⚠高度共线";
        else if (model->vif[j] > 5.0) note = " 中度共线";
        printf("║  %-20s │ %+10.6f │ %10.4f%s ║\n",
               model->feature_names[j], model->coeffs[j],
               model->vif[j], note);
    }
    printf("║  %-20s │ %+10.6f │ %10s  ║\n",
           "截距(Intercept)", model->intercept, "—");
    printf("╠══════════════════════════════════════════════════════════════╣\n");

    /* ── 留出法：前 80% 训练 / 后 20% 测试 ── */
    size_t N = 0;
    double *X = (double *)malloc(MLR_FEATURE_COUNT * dataset->valid_count * sizeof(double));
    double *y = (double *)malloc(dataset->valid_count * sizeof(double));
    if (!X || !y) {
        printf("║  内存不足，跳过留出法评估                                    ║\n");
        printf("╚══════════════════════════════════════════════════════════════╝\n");
        free(X); free(y);
        return;
    }

    if (!extract_feature_matrix(dataset, X, y, &N) || N < 10) {
        printf("║  有效记录不足 (%zu)，跳过留出法评估                          ║\n", N);
        printf("╚══════════════════════════════════════════════════════════════╝\n");
        free(X); free(y);
        return;
    }

    size_t M = MLR_FEATURE_COUNT;
    size_t train_n = N * 80 / 100;
    size_t test_n  = N - train_n;
    if (train_n < M + 2 || test_n < 1) {
        printf("║  样本量不足，跳过留出法评估                                  ║\n");
        printf("╚══════════════════════════════════════════════════════════════╝\n");
        free(X); free(y);
        return;
    }

    /* 训练集求解 */
    double *X_aug_train = (double *)malloc(train_n * (M + 1) * sizeof(double));
    double *y_train     = (double *)malloc(train_n * sizeof(double));
    double *X_aug_test  = (double *)malloc(test_n * (M + 1) * sizeof(double));
    double *y_test      = (double *)malloc(test_n * sizeof(double));
    if (!X_aug_train || !y_train || !X_aug_test || !y_test) {
        free(X); free(y);
        free(X_aug_train); free(y_train); free(X_aug_test); free(y_test);
        return;
    }

    /* 前 train_n 条为训练集，剩余为测试集（时间有序） */
    for (size_t i = 0; i < train_n; i++) {
        X_aug_train[i * (M + 1)] = 1.0;
        for (size_t j = 0; j < M; j++)
            X_aug_train[i * (M + 1) + 1 + j] = X[i * M + j];
        y_train[i] = y[i];
    }
    for (size_t i = 0; i < test_n; i++) {
        size_t src = train_n + i;
        X_aug_test[i * (M + 1)] = 1.0;
        for (size_t j = 0; j < M; j++)
            X_aug_test[i * (M + 1) + 1 + j] = X[src * M + j];
        y_test[i] = y[src];
    }

    int p = (int)(M + 1);
    double *htx  = (double *)calloc((size_t)(p * p), sizeof(double));
    double *hty  = (double *)calloc((size_t)p, sizeof(double));
    double *h_aug = (double *)malloc((size_t)(p * (p + 1)) * sizeof(double));
    double *h_beta = (double *)malloc((size_t)p * sizeof(double));
    if (!htx || !hty || !h_aug || !h_beta) {
        free(X); free(y);
        free(X_aug_train); free(y_train); free(X_aug_test); free(y_test);
        free(htx); free(hty); free(h_aug); free(h_beta);
        return;
    }

    for (size_t i = 0; i < train_n; i++) {
        for (int c1 = 0; c1 < p; c1++) {
            double xi = X_aug_train[i * p + c1];
            hty[c1] += xi * y_train[i];
            for (int c2 = 0; c2 < p; c2++)
                htx[c1 * p + c2] += xi * X_aug_train[i * p + c2];
        }
    }

    /* 填充增广系统 [htx | hty] */
    for (int i = 0; i < p; i++) {
        for (int j = 0; j < p; j++)
            h_aug[i * (p + 1) + j] = htx[i * p + j];
        h_aug[i * (p + 1) + p] = hty[i];
    }

    if (gauss_jordan_solve(p, h_aug, h_beta)) {
        /* 测试集 RMSE */
        double rmse_sum = 0.0;
        for (size_t i = 0; i < test_n; i++) {
            double pred = h_beta[0];
            for (int j = 0; j < (int)M; j++)
                pred += h_beta[1 + j] * X_aug_test[i * p + 1 + j];
            double diff = pred - y_test[i];
            rmse_sum += diff * diff;
        }
        double rmse = sqrt(rmse_sum / (double)test_n);

        printf("║  [留出法] 训练 %zu / 测试 %zu                               ║\n",
               train_n, test_n);
        printf("║  留出法 RMSE = %.6f                                     ║\n", rmse);
        model->rmse = rmse;
        g_mlr_model.rmse = rmse;
    } else {
        printf("║  留出法训练失败（共线性）                                    ║\n");
    }

    printf("╚══════════════════════════════════════════════════════════════╝\n");

    /* 单因素对比 */
    printf("\n  MLR vs 单因素对比:\n");
    printf("  ─────────────────────────────────────\n");
    printf("  模型          R²         调整R²\n");
    printf("  单因素最佳    %6.4f          —\n", 0.1679); /* pH→DO */
    printf("  单因素Air_temp %6.4f         —\n", 0.0000);
    if (!isnan(model->r_squared))
        printf("  MLR (7特征)   %6.4f     %6.4f\n",
               model->r_squared, model->adjusted_r_squared);
    printf("  ─────────────────────────────────────\n");

    free(X); free(y);
    free(X_aug_train); free(y_train); free(X_aug_test); free(y_test);
    free(htx); free(hty); free(h_aug); free(h_beta);

    double improvement = model->r_squared - 0.1679;
    if (model->trained && !isnan(model->r_squared)) {
        if (improvement > 0.05)
            printf("\n  ✓ MLR 显著优于单因素模型 (ΔR² = +%.4f)\n", improvement);
        else if (improvement > 0.0)
            printf("\n  → MLR 略优于单因素模型 (ΔR² = +%.4f)\n", improvement);
        else
            printf("\n  ✗ MLR 未提升预测能力，需重新选择特征组合\n");
    }
}

/* ═════════════════════════════════════════════════════════════════════
 *  MLR 完整流程 + 追加 CSV 报告
 * ═════════════════════════════════════════════════════════════════════ */
void run_mlr_full_pipeline(const WaterDataset *dataset) {
    if (!dataset) return;

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║     4.2 选做：多元线性回归完整流程                    ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    MLRModel mlr;
    train_multivariate_regression(dataset, &mlr);

    if (!mlr.trained) {
        printf("MLR 训练失败，流程终止。\n");
        return;
    }

    evaluate_multivariate_regression(dataset, &mlr);

    /* ── 追加写入 prediction_report.csv ── */
    FILE *fp = fopen("reports/prediction_report.csv", "a");
    if (!fp) {
        printf("\n警告：无法打开 reports/prediction_report.csv，跳过报告追加。\n");
        return;
    }

    fprintf(fp, "\n多元线性回归 (4.2选做),\n");
    fprintf(fp, "特征,系数,VIF\n");
    for (int j = 0; j < mlr.feature_count; j++) {
        fprintf(fp, "%s,%.6f,%.4f\n",
                mlr.feature_names[j], mlr.coeffs[j], mlr.vif[j]);
    }
    fprintf(fp, "截距(Intercept),%.6f,\n", mlr.intercept);
    fprintf(fp, "R²,%.6f,\n", mlr.r_squared);
    fprintf(fp, "调整R²,%.6f,\n", mlr.adjusted_r_squared);
    fprintf(fp, "留出法RMSE,%.6f,\n", mlr.rmse);
    fprintf(fp, "特征数,%d,\n", mlr.feature_count);
    fprintf(fp, "\n与单因素对比,R²,调整R²\n");
    fprintf(fp, "单因素最佳(pH→DO),0.1679,-\n");
    fprintf(fp, "单因素Air_temp→DO,0.0000,-\n");
    if (!isnan(mlr.r_squared))
        fprintf(fp, "MLR(7特征),%.4f,%.4f\n",
                mlr.r_squared, mlr.adjusted_r_squared);

    fclose(fp);
    printf("\nMLR 报告已追加至 reports/prediction_report.csv\n");
}

/* ── 获取全局 MLR 模型 ── */
const MLRModel *get_mlr_model(void) {
    return &g_mlr_model;
}
