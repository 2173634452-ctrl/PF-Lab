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
