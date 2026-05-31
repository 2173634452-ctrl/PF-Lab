#include "preprocess.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：参数类型枚举
 *  用于统一操作六个水质参数，避免为每个参数重复写代码
 * ═════════════════════════════════════════════════════════════════════ */
typedef enum {
    PARAM_TEMP = 0,
    PARAM_SALINITY,
    PARAM_PH,
    PARAM_DO,
    PARAM_PRECIP,
    PARAM_AIR_TEMP
} ParamType;

/* ── 全局预处理统计信息 ── */
static PreprocessStats g_stats;

/* ── 参数名称（中文，用于输出） ── */
static const char *param_names[] = {
    "水温(Temp)", "盐度(Salinity)", "pH",
    "溶解氧(DO)", "降水量(Precip)", "气温(Air_temp)"
};

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：获取记录中某个参数的指针
 * ═════════════════════════════════════════════════════════════════════ */
static double *get_param_ptr(WaterRecord *rec, ParamType param) {
    switch (param) {
        case PARAM_TEMP:      return &rec->temp;
        case PARAM_SALINITY:  return &rec->salinity;
        case PARAM_PH:        return &rec->pH;
        case PARAM_DO:        return &rec->DO;
        case PARAM_PRECIP:    return &rec->precipitation;
        case PARAM_AIR_TEMP:  return &rec->air_temp;
        default:              return NULL;
    }
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：获取记录中某个参数的值（const版本）
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
 *  内部辅助：判断某个参数值是否在合理范围内
 *  合理范围定义见 common.h 中的各 VALID_* 宏
 * ═════════════════════════════════════════════════════════════════════ */
static bool is_param_in_range(ParamType param, double value) {
    /* 先检查是否为 NaN（非数字），NaN 不属于合理范围 */
    if (isnan(value)) return false;

    switch (param) {
        case PARAM_TEMP:
            return value >= VALID_TEMP_MIN && value <= VALID_TEMP_MAX;
        case PARAM_SALINITY:
            return value >= VALID_SALINITY_MIN && value <= VALID_SALINITY_MAX;
        case PARAM_PH:
            return value >= VALID_PH_MIN && value <= VALID_PH_MAX;
        case PARAM_DO:
            return value >= VALID_DO_MIN && value <= VALID_DO_MAX;
        case PARAM_PRECIP:
            return value >= VALID_PRECIP_MIN && value <= VALID_PRECIP_MAX;
        case PARAM_AIR_TEMP:
            return value >= VALID_AIR_TEMP_MIN && value <= VALID_AIR_TEMP_MAX;
        default:
            return false;
    }
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：计算某个参数的全局均值（仅统计有效记录）
 *  用于均值逼近法中两方向均无有效值时的兜底处理
 * ═════════════════════════════════════════════════════════════════════ */
static double compute_param_mean(const WaterDataset *dataset, ParamType param) {
    double sum = 0.0;
    size_t count = 0;

    for (size_t i = 0; i < dataset->total_count; i++) {
        const WaterRecord *rec = &dataset->records[i];
        if (!rec->valid) continue;

        double val = get_param_value(rec, param);
        /* 只统计在合理范围内的值 */
        if (is_param_in_range(param, val)) {
            sum += val;
            count++;
        }
    }

    if (count == 0) return 0.0;
    return sum / (double)count;
}

/* ═════════════════════════════════════════════════════════════════════
 *  核心算法：均值逼近法填充
 *
 *  公式: a_i = (a_{i-n} + a_{i+m}) / 2
 *  其中 a_{i-n} 为前面第 n 个有效数据，a_{i+m} 为后面第 m 个有效数据
 *  默认 n = m = 10
 *
 *  边界处理：
 *    1. 若某方向无有效值，则使用另一方向的有效值
 *    2. 若两方向均无有效值，使用该参数的全集均值
 *
 *  参数说明：
 *    dataset  - 数据集指针
 *    idx      - 需要填充的记录索引
 *    param    - 需要填充的参数类型
 *    n_back   - 向前查找的有效数据个数（默认10）
 *    n_forward- 向后查找的有效数据个数（默认10）
 *  返回值：填充值
 * ═════════════════════════════════════════════════════════════════════ */
static double mean_approximation_fill(const WaterDataset *dataset,
                                       size_t idx, ParamType param,
                                       int n_back, int n_forward) {
    double before_val = NAN;  /* 前面第n_back个有效值 */
    double after_val  = NAN;  /* 后面第n_forward个有效值 */
    int found;

    /* ── 步骤1：向前查找第 n_back 个有效值 ── */
    found = 0;
    for (size_t i = idx; i > 0; ) {
        i--;  /* 先减，因为我们要看 idx 之前的记录 */
        const WaterRecord *rec = &dataset->records[i];
        if (!rec->valid) continue;

        double val = get_param_value(rec, param);
        /* 有效值需满足：非NaN且在合理范围内 */
        if (is_param_in_range(param, val)) {
            found++;
            if (found == n_back) {
                before_val = val;
                break;
            }
        }
    }

    /* ── 步骤2：向后查找第 n_forward 个有效值 ── */
    found = 0;
    for (size_t i = idx + 1; i < dataset->total_count; i++) {
        const WaterRecord *rec = &dataset->records[i];
        if (!rec->valid) continue;

        double val = get_param_value(rec, param);
        if (is_param_in_range(param, val)) {
            found++;
            if (found == n_forward) {
                after_val = val;
                break;
            }
        }
    }

    /* ── 步骤3：根据查找结果返回填充值 ── */
    int before_ok = !isnan(before_val);
    int after_ok  = !isnan(after_val);

    if (before_ok && after_ok) {
        /* 情况1：两方向都有有效值 → 取均值 */
        return (before_val + after_val) / 2.0;
    } else if (before_ok) {
        /* 情况2：仅前方有有效值 → 使用前方值 */
        return before_val;
    } else if (after_ok) {
        /* 情况3：仅后方有有效值 → 使用后方值 */
        return after_val;
    } else {
        /* 情况4：两方向均无有效值 → 使用全局均值 */
        return compute_param_mean(dataset, param);
    }
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：统计一条记录中有几个参数是异常值
 *  返回异常参数个数，同时通过 out_params 数组标记哪些参数异常
 * ═════════════════════════════════════════════════════════════════════ */
static int count_outliers(const WaterRecord *rec, bool out_params[6]) {
    int count = 0;

    /* 依次检查6个参数是否在合理范围内 */
    if (!is_param_in_range(PARAM_TEMP, rec->temp)) {
        out_params[PARAM_TEMP] = true;
        count++;
    }
    if (!is_param_in_range(PARAM_SALINITY, rec->salinity)) {
        out_params[PARAM_SALINITY] = true;
        count++;
    }
    if (!is_param_in_range(PARAM_PH, rec->pH)) {
        out_params[PARAM_PH] = true;
        count++;
    }
    if (!is_param_in_range(PARAM_DO, rec->DO)) {
        out_params[PARAM_DO] = true;
        count++;
    }
    if (!is_param_in_range(PARAM_PRECIP, rec->precipitation)) {
        out_params[PARAM_PRECIP] = true;
        count++;
    }
    if (!is_param_in_range(PARAM_AIR_TEMP, rec->air_temp)) {
        out_params[PARAM_AIR_TEMP] = true;
        count++;
    }

    return count;
}


/* ═════════════════════════════════════════════════════════════════════
 *  2.1 异常值检测与处理
 *
 *  执行流程：
 *    1. 扫描所有有效记录，检测超出合理范围的异常值
 *    2. 统计异常信息（记录数、参数个数、时间跨度）
 *    3. 异常参数 ≥ 3 个 → 整条记录删除
 *    4. 异常参数 < 3 个 → 用均值逼近法填充异常值
 *    5. 更新统计信息并输出结果
 * ═════════════════════════════════════════════════════════════════════ */
void detect_and_handle_outliers(WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供异常值检测。\n");
        return;
    }

    /* 重置统计信息 */
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.outlier_time_start[0] = '\0';
    g_stats.outlier_time_end[0]   = '\0';

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║           2.1 异常值检测与处理                           ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ─────────────────────────────────────────────────────────────
     *  第一遍扫描：统计每条有效记录的异常参数个数
     *  使用动态数组记录每条记录的异常情况，避免重复扫描
     * ───────────────────────────────────────────────────────────── */
    printf("正在扫描异常值...\n");

    /* 为每条记录分配标记数组：是否删除、各参数是否异常 */
    bool *record_to_delete = (bool *)calloc(dataset->total_count, sizeof(bool));
    bool (*outlier_flags)[6] = (bool (*)[6])calloc(dataset->total_count, 6 * sizeof(bool));
    int  *outlier_counts = (int *)calloc(dataset->total_count, sizeof(int));

    if (!record_to_delete || !outlier_flags || !outlier_counts) {
        printf("错误：内存不足，无法进行异常值检测。\n");
        free(record_to_delete);
        free(outlier_flags);
        free(outlier_counts);
        return;
    }

    /* 扫描所有有效记录 */
    for (size_t i = 0; i < dataset->total_count; i++) {
        WaterRecord *rec = &dataset->records[i];

        /* 跳过已标记为无效的记录（含缺失值的记录） */
        if (!rec->valid) continue;

        bool out_params[6] = {false};
        int n_outliers = count_outliers(rec, out_params);

        if (n_outliers > 0) {
            /* 记录异常时间跨度 */
            if (g_stats.outlier_record_count == 0) {
                strncpy(g_stats.outlier_time_start, rec->timestamp, 31);
            }
            strncpy(g_stats.outlier_time_end, rec->timestamp, 31);

            g_stats.outlier_record_count++;
            g_stats.outlier_param_count += n_outliers;

            /* 保存该记录的异常信息 */
            outlier_counts[i] = n_outliers;
            for (int p = 0; p < 6; p++) {
                outlier_flags[i][p] = out_params[p];
            }
        }
    }

    /* ─────────────────────────────────────────────────────────────
     *  输出数据概览（处理前）
     * ───────────────────────────────────────────────────────────── */
    printf("\n┌──────────────────────────────────────────────────────────┐\n");
    printf("│              异常值检测概览                               │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│  总记录数:           %8zu                            │\n", dataset->total_count);
    printf("│  有效记录数:         %8zu                            │\n", dataset->valid_count);
    printf("│  异常数据记录数:     %8zu                            │\n", g_stats.outlier_record_count);
    printf("│  异常数据参数个数:   %8zu                            │\n", g_stats.outlier_param_count);
    printf("│  异常数据时间跨度:   %s ~ %s  │\n",
           g_stats.outlier_time_start, g_stats.outlier_time_end);
    printf("└──────────────────────────────────────────────────────────┘\n");

    if (g_stats.outlier_record_count == 0) {
        printf("\n未检测到异常值，无需处理。\n");
        free(record_to_delete);
        free(outlier_flags);
        free(outlier_counts);
        return;
    }

    /* ─────────────────────────────────────────────────────────────
     *  第二遍处理：删除（≥3异常）或修复（<3异常）
     * ───────────────────────────────────────────────────────────── */

    /* 先标记需要删除的记录 */
    size_t to_delete = 0;
    size_t to_fix = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (outlier_counts[i] >= 3) {
            record_to_delete[i] = true;
            to_delete++;
        } else if (outlier_counts[i] > 0) {
            to_fix++;
        }
    }

    printf("\n处理策略：\n");
    printf("  异常参数 ≥ 3 的记录: %zu 条 → 将整条删除\n", to_delete);
    printf("  异常参数 < 3 的记录: %zu 条 → 将用均值逼近法填充\n", to_fix);

    /* ── 步骤A：修复异常参数 < 3 的记录（先修复，再删除，顺序重要） ── */
    size_t fixed_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (record_to_delete[i]) continue;  /* 跳过要删除的 */
        if (outlier_counts[i] == 0) continue; /* 跳过无异常的 */

        WaterRecord *rec = &dataset->records[i];

        /* 对每个异常参数用均值逼近法填充 */
        for (int p = 0; p < 6; p++) {
            if (outlier_flags[i][p]) {
                double fill_val = mean_approximation_fill(
                    dataset, i, (ParamType)p, 10, 10);
                double *target = get_param_ptr(rec, (ParamType)p);
                *target = fill_val;
            }
        }
        fixed_count++;
    }
    g_stats.fixed_record_count = fixed_count;

    /* ── 步骤B：删除异常参数 ≥ 3 的记录（紧凑数组） ── */
    size_t write_pos = 0;
    size_t deleted_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (record_to_delete[i]) {
            deleted_count++;
            continue;  /* 跳过 → 不保留 */
        }
        if (write_pos != i) {
            dataset->records[write_pos] = dataset->records[i];
        }
        write_pos++;
    }
    g_stats.deleted_record_count = deleted_count;
    dataset->total_count = write_pos;

    /* 重新计算有效记录数 */
    dataset->valid_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) {
            dataset->valid_count++;
        }
    }

    /* ─────────────────────────────────────────────────────────────
     *  输出处理结果
     * ───────────────────────────────────────────────────────────── */
    printf("\n┌──────────────────────────────────────────────────────────┐\n");
    printf("│              异常值处理结果                               │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│  修复异常值记录数:   %8zu                            │\n", g_stats.fixed_record_count);
    printf("│  删除异常值记录数:   %8zu                            │\n", g_stats.deleted_record_count);
    printf("│  处理后总记录数:     %8zu                            │\n", dataset->total_count);
    printf("│  处理后有效记录数:   %8zu                            │\n", dataset->valid_count);
    printf("└──────────────────────────────────────────────────────────┘\n");

    /* 释放临时数组 */
    free(record_to_delete);
    free(outlier_flags);
    free(outlier_counts);
}


/* ═════════════════════════════════════════════════════════════════════
 *  2.2 缺失值处理——均值逼近法
 *
 *  处理数据中的缺失值（NaN、-999、-9999、空值等）
 *  这些缺失值在加载时已导致 record->valid = false
 *  本函数遍历所有无效记录，对每个缺失参数用均值逼近法填充
 *
 *  均值逼近法公式: a_i = (a_{i-n} + a_{i+m}) / 2
 *  默认 n = m = 10
 *
 *  边界处理：
 *    1. 某方向无有效值 → 使用另一方向的值
 *    2. 两方向均无有效值 → 使用全局均值
 * ═════════════════════════════════════════════════════════════════════ */
void fill_missing_values(WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供缺失值处理。\n");
        return;
    }

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║         2.2 缺失值处理——均值逼近法                      ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* 统计无效记录数 */
    size_t invalid_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (!dataset->records[i].valid) {
            invalid_count++;
        }
    }

    if (invalid_count == 0) {
        printf("未检测到缺失值，无需处理。\n");
        g_stats.missing_value_count = 0;
        return;
    }

    printf("检测到 %zu 条包含缺失值的记录。\n", invalid_count);
    printf("正在使用均值逼近法填充（n=m=10）...\n");

    /* ─────────────────────────────────────────────────────────────
     *  遍历所有记录，对无效记录中的每个缺失参数进行填充
     *
     *  判断某个参数是否缺失的方法：
     *    检查参数值是否为 NaN（在加载阶段，缺失值被解析为 NaN）
     *    注意：isnan 是 math.h 中的标准宏，用于判断浮点数是否为 NaN
     * ───────────────────────────────────────────────────────────── */
    size_t filled_count = 0;  /* 填充的参数总个数 */

    for (size_t i = 0; i < dataset->total_count; i++) {
        WaterRecord *rec = &dataset->records[i];

        /* 只处理标记为无效的记录 */
        if (rec->valid) continue;

        /* 逐一检查每个参数，对缺失值（NaN）进行填充 */
        ParamType params[] = {
            PARAM_TEMP, PARAM_SALINITY, PARAM_PH,
            PARAM_DO, PARAM_PRECIP, PARAM_AIR_TEMP
        };

        for (int p = 0; p < 6; p++) {
            ParamType param = params[p];
            double val = get_param_value(rec, param);

            /* isnan 判断是否为 NaN（非数字），即数据加载时无法解析的值 */
            if (isnan(val)) {
                double fill_val = mean_approximation_fill(
                    dataset, i, param, 10, 10);
                double *target = get_param_ptr(rec, param);
                *target = fill_val;
                filled_count++;
            }
        }

        /* 填充完毕后，将该记录标记为有效 */
        rec->valid = true;
    }

    /* 重新计算有效记录数 */
    dataset->valid_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) {
            dataset->valid_count++;
        }
    }

    g_stats.missing_value_count = filled_count;

    printf("\n┌──────────────────────────────────────────────────────────┐\n");
    printf("│              缺失值处理结果                               │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│  处理的缺失值个数:   %8zu                            │\n", filled_count);
    printf("│  处理后总记录数:     %8zu                            │\n", dataset->total_count);
    printf("│  处理后有效记录数:   %8zu                            │\n", dataset->valid_count);
    printf("└──────────────────────────────────────────────────────────┘\n");
}


/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：计算一组数据的标准差
 *
 *  标准差公式: σ = sqrt( Σ(x_i - x̄)² / n )
 *  用于滤波前后对比，衡量数据的波动程度
 * ═════════════════════════════════════════════════════════════════════ */
static double compute_stddev(const double *values, size_t count) {
    if (count == 0) return 0.0;

    /* 第一步：计算均值 */
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += values[i];
    }
    double mean = sum / (double)count;

    /* 第二步：计算方差 */
    double variance_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = values[i] - mean;
        variance_sum += diff * diff;
    }

    /* 第三步：返回标准差（方差的平方根） */
    return sqrt(variance_sum / (double)count);
}

/* ═════════════════════════════════════════════════════════════════════
 *  2.3 移动平均滤波
 *
 *  公式: y[i] = (x[i-k] + ... + x[i] + ... + x[i+k]) / N
 *  其中 N = 2k+1 为窗口大小，k = (N-1)/2
 *
 *  滤波对象：水温、溶解氧、pH、盐度 四个参数
 *
 *  边界处理：
 *    在数组开头或结尾，向前或向后取值不足 k 个时，
 *    只取可用的数据点，除以实际取到的数据点数。
 *    例如：i=0 时只取 i=0 到 i=k 共 k+1 个值，除以 k+1。
 * ═════════════════════════════════════════════════════════════════════ */
void apply_moving_average(WaterDataset *dataset, int window_size) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供滤波。\n");
        return;
    }

    /* 窗口必须为奇数 */
    if (window_size % 2 == 0) {
        printf("警告：窗口大小必须为奇数，已自动调整为 %d。\n", window_size + 1);
        window_size += 1;
    }
    if (window_size < 3) {
        printf("警告：窗口大小至少为3，已自动调整为3。\n");
        window_size = 3;
    }

    int k = (window_size - 1) / 2;  /* 半窗口大小 */
    size_t n = dataset->total_count;

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║         2.3 移动平均滤波（窗口 = %d）                    ║\n", window_size);
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ─────────────────────────────────────────────────────────────
     *  对四个核心参数分别进行滤波：
     *    水温(Temp)、溶解氧(DO)、pH、盐度(Salinity)
     *
     *  每个参数的滤波流程：
     *    1. 提取原始值到临时数组
     *    2. 计算滤波前标准差
     *    3. 应用移动平均
     *    4. 计算滤波后标准差
     *    5. 输出对比结果
     * ───────────────────────────────────────────────────────────── */
    ParamType filter_params[] = {PARAM_TEMP, PARAM_DO, PARAM_PH, PARAM_SALINITY};

    /* 分配临时数组：用于保存原始值和滤波后的值 */
    double *original = (double *)malloc(sizeof(double) * n);
    double *filtered = (double *)malloc(sizeof(double) * n);

    if (!original || !filtered) {
        printf("错误：内存不足，无法进行滤波。\n");
        free(original);
        free(filtered);
        return;
    }

    for (int pi = 0; pi < 4; pi++) {
        ParamType param = filter_params[pi];

        /* ── 步骤1：提取原始数据 ── */
        for (size_t i = 0; i < n; i++) {
            original[i] = get_param_value(&dataset->records[i], param);
        }

        /* ── 步骤2：计算滤波前标准差 ── */
        double stddev_before = compute_stddev(original, n);

        /* ── 步骤3：应用移动平均滤波 ── */
        for (size_t i = 0; i < n; i++) {
            /* 确定滤波窗口的实际范围（处理边界） */
            size_t start = (i >= (size_t)k) ? (i - k) : 0;
            size_t end   = (i + k < n) ? (i + k) : (n - 1);

            /* 计算窗口内的平均值 */
            double sum = 0.0;
            size_t count = 0;
            for (size_t j = start; j <= end; j++) {
                sum += original[j];
                count++;
            }

            filtered[i] = (count > 0) ? (sum / (double)count) : original[i];
        }

        /* ── 步骤4：计算滤波后标准差 ── */
        double stddev_after = compute_stddev(filtered, n);

        /* ── 步骤5：将滤波结果写回数据集 ── */
        for (size_t i = 0; i < n; i++) {
            double *target = get_param_ptr(&dataset->records[i], param);
            *target = filtered[i];
        }

        /* ── 步骤6：输出对比 ── */
        double reduction = stddev_before > 0
            ? (1.0 - stddev_after / stddev_before) * 100.0
            : 0.0;

        printf("  %-20s  滤波前标准差: %.4f  →  滤波后标准差: %.4f  (噪声降低 %.1f%%)\n",
               param_names[param], stddev_before, stddev_after, reduction);
    }

    free(original);
    free(filtered);

    printf("\n移动平均滤波完成（窗口 = %d）。\n", window_size);
}


/* ═════════════════════════════════════════════════════════════════════
 *  交互式移动平均滤波
 *
 *  让用户选择窗口大小（3/5/7/9/11），然后：
 *    1. 对四个参数分别进行滤波
 *    2. 显示滤波前后标准差对比
 *    3. 对比不同窗口的降噪效果
 *
 *  用于实验分析，帮助理解窗口大小与噪声抑制的关系：
 *    窗口越大 → 曲线越平滑 → 噪声降低越多
 *    但窗口过大 → 可能丢失短期变化细节（过度平滑）
 * ═════════════════════════════════════════════════════════════════════ */
void interactive_moving_average(WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供滤波。\n");
        return;
    }

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║         2.3 移动平均滤波 — 窗口选择                     ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  可选窗口: 3 / 5 / 7 / 9 / 11                          ║\n");
    printf("║  窗口越大，曲线越平滑，但可能丢失短期变化细节          ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n请输入窗口大小: ");

    int window;
    if (scanf("%d", &window) != 1) {
        printf("输入无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    /* 验证窗口大小 */
    if (window != 3 && window != 5 && window != 7 && window != 9 && window != 11) {
        printf("窗口大小必须为 3/5/7/9/11 之一。\n");
        return;
    }

    /* 执行滤波 */
    apply_moving_average(dataset, window);

    /* ─────────────────────────────────────────────────────────────
     *  分析讨论提示：
     *  帮助学生理解窗口大小与滤波效果的关系
     * ───────────────────────────────────────────────────────────── */
    printf("\n┌──────────────────────────────────────────────────────────┐\n");
    printf("│  分析讨论：窗口大小与噪声抑制的关系                      │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│  窗口=3  → 轻度平滑，保留短期波动                       │\n");
    printf("│  窗口=5  → 中度平滑，滤除高频噪声                       │\n");
    printf("│  窗口=7  → 较强平滑，趋势更明显                         │\n");
    printf("│  窗口=9  → 强平滑，适合分析长期趋势                     │\n");
    printf("│  窗口=11 → 最强平滑，可能丢失短期变化                   │\n");
    printf("│                                                          │\n");
    printf("│  建议：可多次尝试不同窗口，观察标准差降低程度。          │\n");
    printf("│  通常窗口5或7能在平滑度和细节保留之间取得较好平衡。      │\n");
    printf("└──────────────────────────────────────────────────────────┘\n");
}


/* ═════════════════════════════════════════════════════════════════════
 *  获取预处理统计信息（供外部模块查询）
 * ═════════════════════════════════════════════════════════════════════ */
const PreprocessStats *get_preprocess_stats(void) {
    return &g_stats;
}


/* ═════════════════════════════════════════════════════════════════════
 *  将预处理统计信息追加写入数据概览文件
 *
 *  根据任务书要求，以下信息需写入数据概览：
 *    - 异常数据记录数、异常数据错误参数个数
 *    - 修复异常值记录数、删除异常值记录数
 *    - 处理的缺失值个数
 * ═════════════════════════════════════════════════════════════════════ */
void append_preprocess_overview(const WaterDataset *dataset) {
    if (!dataset) return;

    /* 以追加模式打开概览文件 */
    FILE *fp = fopen("reports/data_overview.txt", "a");
    if (!fp) {
        printf("警告：无法写入数据概览文件。\n");
        return;
    }

    fprintf(fp, "\n──────────────────────────────────────────\n");
    fprintf(fp, "        预处理统计（模块二）\n");
    fprintf(fp, "──────────────────────────────────────────\n");

    /* 异常值检测统计 */
    fprintf(fp, "\n[2.1 异常值检测与处理]\n");
    fprintf(fp, "  异常数据记录数:       %zu\n", g_stats.outlier_record_count);
    fprintf(fp, "  异常数据参数个数:     %zu\n", g_stats.outlier_param_count);
    if (g_stats.outlier_time_start[0] != '\0') {
        fprintf(fp, "  异常数据时间跨度:     %s ~ %s\n",
                g_stats.outlier_time_start, g_stats.outlier_time_end);
    }
    fprintf(fp, "  修复异常值记录数:     %zu\n", g_stats.fixed_record_count);
    fprintf(fp, "  删除异常值记录数:     %zu\n", g_stats.deleted_record_count);

    /* 缺失值处理统计 */
    fprintf(fp, "\n[2.2 缺失值处理——均值逼近法]\n");
    fprintf(fp, "  处理的缺失值个数:     %zu\n", g_stats.missing_value_count);

    /* 当前数据集状态 */
    fprintf(fp, "\n[处理后数据集状态]\n");
    fprintf(fp, "  总记录数:             %zu\n", dataset->total_count);
    fprintf(fp, "  有效记录数:           %zu\n", dataset->valid_count);
    if (dataset->total_count > 0) {
        fprintf(fp, "  数据有效率:           %.2f%%\n",
                100.0 * dataset->valid_count / dataset->total_count);
    }

    fprintf(fp, "\n──────────────────────────────────────────\n");

    fclose(fp);
    printf("预处理统计信息已追加写入 reports/data_overview.txt\n");
}
