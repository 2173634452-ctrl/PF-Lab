#include "analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：参数类型枚举（与 preprocess.c 对齐）
 * ═════════════════════════════════════════════════════════════════════ */
typedef enum {
    PARAM_TEMP = 0,
    PARAM_SALINITY,
    PARAM_PH,
    PARAM_DO,
    PARAM_PRECIP,
    PARAM_AIR_TEMP
} ParamType;

static const char *param_names[] = {
    "水温(Temp)", "盐度(Salinity)", "pH",
    "溶解氧(DO)", "降水量(Precip)", "气温(Air_temp)"
};

static const char *param_keys[] = {
    "Temp", "Salinity", "pH", "DO", "Precip", "Air_temp"
};

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：统计函数（单参数数组版本）
 * ═════════════════════════════════════════════════════════════════════ */

static double compute_mean(const double *values, size_t count) {
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) sum += values[i];
    return sum / (double)count;
}

static double compute_max(const double *values, size_t count) {
    if (count == 0) return 0.0;
    double max_val = values[0];
    for (size_t i = 1; i < count; i++)
        if (values[i] > max_val) max_val = values[i];
    return max_val;
}

static double compute_min(const double *values, size_t count) {
    if (count == 0) return 0.0;
    double min_val = values[0];
    for (size_t i = 1; i < count; i++)
        if (values[i] < min_val) min_val = values[i];
    return min_val;
}

static double compute_stddev(const double *values, size_t count) {
    if (count == 0) return 0.0;
    double mean = compute_mean(values, count);
    double variance_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = values[i] - mean;
        variance_sum += diff * diff;
    }
    return sqrt(variance_sum / (double)count);
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：从时间戳提取小时和分钟
 *  格式 "YYYY-MM-DD HH:MM" → 时/分在固定位置
 * ═════════════════════════════════════════════════════════════════════ */
static int extract_hour(const char *timestamp) {
    /* timestamp[11]='H', timestamp[12]='H' → 如 "03" */
    return (timestamp[11] - '0') * 10 + (timestamp[12] - '0');
}

/* 提取日期部分（前10个字符）用于按天分组 */
static void extract_date(const char *timestamp, char date[11]) {
    strncpy(date, timestamp, 10);
    date[10] = '\0';
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：写入警告到 CSV 文件
 * ═════════════════════════════════════════════════════════════════════ */
static void write_warning(FILE *fp, const char *type, const char *time_str,
                          const char *level, const char *advice) {
    fprintf(fp, "%s,%s,%s,\"%s\"\n", type, time_str, level, advice);
}

/* ═════════════════════════════════════════════════════════════════════
 *  3.1 基本统计量
 *
 *  对6个水质参数分别计算均值、最大值、最小值、标准差。
 *  仅使用有效记录（valid == true）。
 *  结果同时输出到控制台和 CSV 文件。
 * ═════════════════════════════════════════════════════════════════════ */
void compute_statistics(const WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供统计分析。\n");
        return;
    }

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║           3.1 基本统计量                                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ── 步骤1：提取各参数的有效值数组 ── */
    size_t valid_n = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) valid_n++;
    }
    if (valid_n == 0) {
        printf("错误：无有效记录。\n");
        return;
    }

    /* 为每个参数分配数组 */
    double *param_arrays[6];
    for (int p = 0; p < 6; p++) {
        param_arrays[p] = (double *)malloc(sizeof(double) * valid_n);
        if (!param_arrays[p]) {
            printf("错误：内存不足。\n");
            for (int q = 0; q < p; q++) free(param_arrays[q]);
            return;
        }
    }

    /* 填充各参数数组（仅有效记录） */
    size_t idx = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (!dataset->records[i].valid) continue;
        const WaterRecord *rec = &dataset->records[i];
        param_arrays[PARAM_TEMP][idx]      = rec->temp;
        param_arrays[PARAM_SALINITY][idx]  = rec->salinity;
        param_arrays[PARAM_PH][idx]        = rec->pH;
        param_arrays[PARAM_DO][idx]        = rec->DO;
        param_arrays[PARAM_PRECIP][idx]    = rec->precipitation;
        param_arrays[PARAM_AIR_TEMP][idx]  = rec->air_temp;
        idx++;
    }

    /* ── 步骤2：计算并输出 ── */
    int col_widths[] = {20, 12, 12, 12, 12};
    printf("  %-*s %*s %*s %*s %*s\n",
           col_widths[0], "参数",
           col_widths[1], "均值",
           col_widths[2], "最大值",
           col_widths[3], "最小值",
           col_widths[4], "标准差");
    printf("  %-*s %*s %*s %*s %*s\n",
           col_widths[0], "────────────────────",
           col_widths[1], "──────────",
           col_widths[2], "──────────",
           col_widths[3], "──────────",
           col_widths[4], "──────────");

    /* 写入 CSV 文件 */
    FILE *fp = fopen("reports/statistics_report.csv", "w");
    if (fp) {
        fprintf(fp, "参数,均值,最大值,最小值,标准差\n");
    }

    for (int p = 0; p < 6; p++) {
        double mean   = compute_mean(param_arrays[p], valid_n);
        double max_v  = compute_max(param_arrays[p], valid_n);
        double min_v  = compute_min(param_arrays[p], valid_n);
        double stddev = compute_stddev(param_arrays[p], valid_n);

        printf("  %-*s %*.4f %*.4f %*.4f %*.4f\n",
               col_widths[0], param_names[p],
               col_widths[1], mean,
               col_widths[2], max_v,
               col_widths[3], min_v,
               col_widths[4], stddev);

        if (fp) {
            fprintf(fp, "%s,%.4f,%.4f,%.4f,%.4f\n",
                    param_keys[p], mean, max_v, min_v, stddev);
        }
    }

    printf("\n  统计样本数: %zu 条有效记录\n", valid_n);

    if (fp) {
        fprintf(fp, "\n统计样本数: %zu 条有效记录\n", valid_n);
        fclose(fp);
        printf("\n基本统计量已写入 reports/statistics_report.csv\n");
    }

    /* 清理 */
    for (int p = 0; p < 6; p++) free(param_arrays[p]);
}

/* ═════════════════════════════════════════════════════════════════════
 *  3.2 凌晨缺氧预警
 *
 *  数据从 2025-01-01 12:00 开始，每5分钟采集一次（每天288条）。
 *  筛选每天 03:00-05:00 的 DO 数据（共25条：03:00,03:05,...,04:55）。
 *  若 DO 均值 < 4.0 → 亚缺氧预警；若 < 3.0 → 严重缺氧预警。
 *
 *  按天分组：提取日期部分（前10位），累计每天的 DO 值。
 * ═════════════════════════════════════════════════════════════════════ */
void hypoxia_warning(const WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供缺氧预警分析。\n");
        return;
    }

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║         3.2 凌晨缺氧预警 (03:00-05:00)                   ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    printf("  正在分析每日凌晨 03:00-05:00 的溶解氧数据...\n\n");

    /* 写入 CSV 预警报告 */
    FILE *fp = fopen("reports/warning_report.csv", "w");
    if (fp) {
        fprintf(fp, "预警类型,日期,预警等级,处理建议\n");
    }

    /* ── 按天遍历 ── */
    size_t i = 0;
    int day_count = 0;
    int warning_count = 0;

    while (i < dataset->total_count) {
        /* 获取当前日期 */
        char current_date[11];
        extract_date(dataset->records[i].timestamp, current_date);

        /* 收集当天 03:00-05:00 的 DO 数据 */
        double do_sum = 0.0;
        int do_count = 0;

        /* 处理当前日期内的所有记录 */
        while (i < dataset->total_count) {
            char rec_date[11];
            extract_date(dataset->records[i].timestamp, rec_date);
            if (strcmp(rec_date, current_date) != 0) break;

            const WaterRecord *rec = &dataset->records[i];
            if (rec->valid) {
                int h = extract_hour(rec->timestamp);

                /* 03:00 ≤ time ≤ 04:55（含） */
                if ((h == 3) || (h == 4)) {
                    do_sum += rec->DO;
                    do_count++;
                }
            }
            i++;
        }

        day_count++;

        /* 判断预警 */
        if (do_count > 0) {
            double do_mean = do_sum / (double)do_count;

            if (do_mean < 3.0) {
                warning_count++;
                printf("  ⚠ %s | DO均值=%.2f | 严重缺氧预警 | 需立即投放颗粒氧并减少投喂\n",
                       current_date, do_mean);
                if (fp) write_warning(fp, "凌晨缺氧", current_date,
                                      "严重缺氧预警",
                                      "需立即投放颗粒氧并减少投喂");
            } else if (do_mean < 4.0) {
                warning_count++;
                printf("  ⚠ %s | DO均值=%.2f | 亚缺氧预警   | 建议开启底部增氧机\n",
                       current_date, do_mean);
                if (fp) write_warning(fp, "凌晨缺氧", current_date,
                                      "亚缺氧预警",
                                      "建议开启底部增氧机");
            }
        }
    }

    if (warning_count == 0) {
        printf("  分析 %d 天数据，未触发凌晨缺氧预警。\n", day_count);
        if (fp) fprintf(fp, "未触发凌晨缺氧预警\n");
    } else {
        printf("\n  分析 %d 天数据，共触发 %d 条预警。\n", day_count, warning_count);
    }

    if (fp) {
        fclose(fp);
        printf("  预警报告已写入 reports/warning_report.csv\n");
    }
}

/* ═════════════════════════════════════════════════════════════════════
 *  3.2 盐度突变预警
 *
 *  1小时变化率：|盐度(t) - 盐度(t-12)| > 2 PSU → 预警
 *    其中 t-12 为 1 小时前（12 条记录前，每5分钟一条）
 *
 *  24小时累计降幅：24小时窗口内 max - min > 5 PSU → 预警
 *    24小时 = 288 条记录
 * ═════════════════════════════════════════════════════════════════════ */
void salinity_warning(const WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供盐度突变预警分析。\n");
        return;
    }

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║         3.2 盐度突变预警                                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* 追加模式打开预警报告（缺氧预警可能已创建） */
    FILE *fp = fopen("reports/warning_report.csv", "a");
    /* 如果文件不存在则创建并写表头 */
    if (!fp) {
        fp = fopen("reports/warning_report.csv", "w");
        if (fp) fprintf(fp, "预警类型,日期时间,预警等级,处理建议\n");
    }

    int warning_1h = 0;    /* 1小时突变预警计数 */
    int warning_24h = 0;   /* 24小时累计预警计数 */

    /* ── 1小时变化率检测 ── */
    printf("  [1] 检测1小时盐度突变 (阈值: > 2 PSU)...\n");

    for (size_t i = 12; i < dataset->total_count; i++) {
        const WaterRecord *rec_cur  = &dataset->records[i];
        const WaterRecord *rec_prev = &dataset->records[i - 12];

        if (!rec_cur->valid || !rec_prev->valid) continue;

        double diff = fabs(rec_cur->salinity - rec_prev->salinity);
        if (diff > 2.0) {
            warning_1h++;
            printf("  ⚠ 1小时盐度突变 | %s | 变化 %.2f PSU | 建议关闭进水口，泼洒高稳VC\n",
                   rec_cur->timestamp, diff);
            if (fp) {
                char label[64];
                snprintf(label, sizeof(label), "盐度突变(1h)");
                write_warning(fp, label, rec_cur->timestamp,
                              "盐度突变预警",
                              "建议关闭进水口，泼洒高稳VC或葡萄糖");
            }
        }
    }
    if (warning_1h == 0) {
        printf("     未检测到1小时盐度突变。\n");
    }

    /* ── 24小时累计降幅检测 ── */
    printf("\n  [2] 检测24小时累计盐度降幅 (阈值: > 5 PSU)...\n");

    /* 24小时 = 288条记录。滑动窗口：对每个起始点i，
     * 检查从 i 到 i+287 的盐度最大最小值差异。
     * 为效率考虑，按天（288条为单位）进行批处理。 */
    size_t day_size = 288;
    for (size_t start = 0; start < dataset->total_count; start += day_size) {
        size_t end = start + day_size;
        if (end > dataset->total_count) end = dataset->total_count;

        double sal_min = 1e9, sal_max = -1e9;
        bool has_valid = false;

        for (size_t i = start; i < end; i++) {
            const WaterRecord *rec = &dataset->records[i];
            if (!rec->valid) continue;
            if (rec->salinity < sal_min) sal_min = rec->salinity;
            if (rec->salinity > sal_max) sal_max = rec->salinity;
            has_valid = true;
        }

        if (has_valid && (sal_max - sal_min > 5.0)) {
            warning_24h++;
            printf("  ⚠ 24h盐度累计降幅 | %s ~ %s | 降幅 %.2f PSU | 建议关闭进水口，泼洒高稳VC\n",
                   dataset->records[start].timestamp,
                   dataset->records[end - 1].timestamp,
                   sal_max - sal_min);
            if (fp) {
                char label[64];
                snprintf(label, sizeof(label), "盐度突变(24h累计)");
                write_warning(fp, label, dataset->records[start].timestamp,
                              "盐度突变预警",
                              "建议关闭进水口，泼洒高稳VC或葡萄糖");
            }
        }
    }
    if (warning_24h == 0) {
        printf("     未检测到24小时盐度累计突变。\n");
    }

    printf("\n  盐度突变预警完成：1小时突变 %d 次，24小时累计突变 %d 次。\n",
           warning_1h, warning_24h);

    if (fp) {
        fclose(fp);
        printf("  预警报告已追加写入 reports/warning_report.csv\n");
    }
}

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助：皮尔逊相关系数计算
 *
 *  公式: r = (n*Σxy - Σx*Σy) / sqrt( (n*Σx²-(Σx)²) * (n*Σy²-(Σy)²) )
 *
 *  返回 NaN 表示无法计算（样本数不足、方差为零等）
 * ═════════════════════════════════════════════════════════════════════ */
static double pearson_corr(const double *x, const double *y, size_t n) {
    if (n < 2) return NAN;

    double sum_x = 0.0, sum_y = 0.0;
    double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;

    for (size_t i = 0; i < n; i++) {
        sum_x  += x[i];
        sum_y  += y[i];
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
        sum_y2 += y[i] * y[i];
    }

    double numerator   = (double)n * sum_xy - sum_x * sum_y;
    double denom_left  = (double)n * sum_x2 - sum_x * sum_x;
    double denom_right = (double)n * sum_y2 - sum_y * sum_y;

    /* 方差为零时，分母为0 → 返回 NaN */
    if (denom_left <= 0.0 || denom_right <= 0.0) return NAN;

    return numerator / sqrt(denom_left * denom_right);
}

/* ═════════════════════════════════════════════════════════════════════
 *  3.3 皮尔逊相关系数矩阵
 *
 *  计算6个参数两两之间的皮尔逊相关系数，输出 6×6 矩阵。
 *  找出最强正相关和最强负相关（排除对角线 1.0）。
 *  分析四组特定参数对：水温-DO, pH-DO, 水温-气温, 水温-盐度。
 *
 *  同时将结果写入外部矩阵供其他模块使用。
 * ═════════════════════════════════════════════════════════════════════ */
void generate_correlation_matrix(const WaterDataset *dataset, double matrix[6][6]) {
    if (!dataset || dataset->total_count == 0) {
        printf("错误：无数据可供相关性分析。\n");
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 6; j++)
                matrix[i][j] = 0.0;
        return;
    }

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║         3.3 皮尔逊相关系数分析                           ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ── 步骤1：提取各参数的有效值数组 ── */
    size_t valid_n = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) valid_n++;
    }
    if (valid_n < 2) {
        printf("错误：有效样本数不足（需 ≥ 2）。\n");
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 6; j++)
                matrix[i][j] = 0.0;
        return;
    }

    double *param_arrays[6];
    for (int p = 0; p < 6; p++) {
        param_arrays[p] = (double *)malloc(sizeof(double) * valid_n);
        if (!param_arrays[p]) {
            printf("错误：内存不足。\n");
            for (int q = 0; q < p; q++) free(param_arrays[q]);
            return;
        }
    }

    size_t idx = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (!dataset->records[i].valid) continue;
        const WaterRecord *rec = &dataset->records[i];
        param_arrays[0][idx] = rec->temp;
        param_arrays[1][idx] = rec->salinity;
        param_arrays[2][idx] = rec->pH;
        param_arrays[3][idx] = rec->DO;
        param_arrays[4][idx] = rec->precipitation;
        param_arrays[5][idx] = rec->air_temp;
        idx++;
    }

    /* ── 步骤2：计算 6×6 相关系数矩阵 ── */
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            if (i == j) {
                matrix[i][j] = 1.0;
            } else {
                double r = pearson_corr(param_arrays[i], param_arrays[j], valid_n);
                matrix[i][j] = r;
            }
        }
    }

    /* ── 步骤3：控制台输出矩阵 ── */
    printf("  6×6 皮尔逊相关系数矩阵:\n\n");
    printf("  %-16s", "");
    for (int j = 0; j < 6; j++) printf(" %8s", param_keys[j]);
    printf("\n");

    for (int i = 0; i < 6; i++) {
        printf("  %-16s", param_keys[i]);
        for (int j = 0; j < 6; j++) {
            if (isnan(matrix[i][j]))
                printf(" %8s", "N/A");
            else
                printf(" %8.4f", matrix[i][j]);
        }
        printf("\n");
    }

    /* ── 步骤4：写入 CSV ── */
    FILE *fp = fopen("reports/statistics_report.csv", "a");
    if (fp) {
        fprintf(fp, "\n6×6 皮尔逊相关系数矩阵\n");
        fprintf(fp, ",");
        for (int j = 0; j < 6; j++) fprintf(fp, "%s%s", param_keys[j], j < 5 ? "," : "\n");
        for (int i = 0; i < 6; i++) {
            fprintf(fp, "%s", param_keys[i]);
            for (int j = 0; j < 6; j++) {
                if (isnan(matrix[i][j]))
                    fprintf(fp, ",N/A");
                else
                    fprintf(fp, ",%.4f", matrix[i][j]);
            }
            fprintf(fp, "\n");
        }
    }

    /* ── 步骤5：找出最强正相关和最强负相关 ── */
    double max_r = -2.0, min_r = 2.0;
    int max_i = 0, max_j = 0, min_i = 0, min_j = 0;

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            if (i == j) continue;
            if (isnan(matrix[i][j])) continue;
            if (matrix[i][j] > max_r) { max_r = matrix[i][j]; max_i = i; max_j = j; }
            if (matrix[i][j] < min_r) { min_r = matrix[i][j]; min_i = i; min_j = j; }
        }
    }

    printf("\n  ┌────────────────────────────────────────────┐\n");
    printf("  │  相关性极值                                 │\n");
    printf("  ├────────────────────────────────────────────┤\n");
    if (max_r > -2.0)
        printf("  │  最强正相关: %-6s ↔ %-6s  r = %+.4f  │\n",
               param_keys[max_i], param_keys[max_j], max_r);
    if (min_r < 2.0)
        printf("  │  最强负相关: %-6s ↔ %-6s  r = %+.4f  │\n",
               param_keys[min_i], param_keys[min_j], min_r);
    printf("  └────────────────────────────────────────────┘\n");

    /* ── 步骤6：特定参数对分析 ── */
    printf("\n  ┌────────────────────────────────────────────┐\n");
    printf("  │  关键参数对分析                             │\n");
    printf("  ├────────────────────────────────────────────┤\n");

    /* 水温-DO：PARAM_TEMP(0) ↔ PARAM_DO(3) */
    double r_temp_do = matrix[0][3];
    printf("  │  水温 ↔ 溶解氧:      r = %+.4f", r_temp_do);
    if (!isnan(r_temp_do)) {
        if (fabs(r_temp_do) >= 0.6) printf("  (强相关)");
        else if (fabs(r_temp_do) >= 0.4) printf("  (中等相关)");
        else if (fabs(r_temp_do) >= 0.2) printf("  (弱相关)");
        else printf("  (极弱/无关)");
    }
    printf("     │\n");

    /* pH-DO：PARAM_PH(2) ↔ PARAM_DO(3) */
    double r_ph_do = matrix[2][3];
    printf("  │  pH ↔ 溶解氧:        r = %+.4f", r_ph_do);
    if (!isnan(r_ph_do)) {
        if (fabs(r_ph_do) >= 0.6) printf("  (强相关)");
        else if (fabs(r_ph_do) >= 0.4) printf("  (中等相关)");
        else if (fabs(r_ph_do) >= 0.2) printf("  (弱相关)");
        else printf("  (极弱/无关)");
    }
    printf("     │\n");

    /* 水温-气温：PARAM_TEMP(0) ↔ PARAM_AIR_TEMP(5) */
    double r_temp_air = matrix[0][5];
    printf("  │  水温 ↔ 气温:        r = %+.4f", r_temp_air);
    if (!isnan(r_temp_air)) {
        if (fabs(r_temp_air) >= 0.6) printf("  (强相关)");
        else if (fabs(r_temp_air) >= 0.4) printf("  (中等相关)");
        else if (fabs(r_temp_air) >= 0.2) printf("  (弱相关)");
        else printf("  (极弱/无关)");
    }
    printf("     │\n");

    /* 水温-盐度：PARAM_TEMP(0) ↔ PARAM_SALINITY(1) */
    double r_temp_sal = matrix[0][1];
    printf("  │  水温 ↔ 盐度:        r = %+.4f", r_temp_sal);
    if (!isnan(r_temp_sal)) {
        if (fabs(r_temp_sal) >= 0.6) printf("  (强相关)");
        else if (fabs(r_temp_sal) >= 0.4) printf("  (中等相关)");
        else if (fabs(r_temp_sal) >= 0.2) printf("  (弱相关)");
        else printf("  (极弱/无关)");
    }
    printf("     │\n");
    printf("  └────────────────────────────────────────────┘\n");

    /* ── 步骤7：追加 CSV 结论 ── */
    if (fp) {
        fprintf(fp, "\n相关性分析结论\n");
        fprintf(fp, "最强正相关,%s ↔ %s,r=%.4f\n",
                param_keys[max_i], param_keys[max_j], max_r);
        fprintf(fp, "最强负相关,%s ↔ %s,r=%.4f\n",
                param_keys[min_i], param_keys[min_j], min_r);
        fprintf(fp, "水温 ↔ 溶解氧,r=%.4f\n", r_temp_do);
        fprintf(fp, "pH ↔ 溶解氧,r=%.4f\n", r_ph_do);
        fprintf(fp, "水温 ↔ 气温,r=%.4f\n", r_temp_air);
        fprintf(fp, "水温 ↔ 盐度,r=%.4f\n", r_temp_sal);
        fclose(fp);
        printf("\n  相关系数矩阵已追加写入 reports/statistics_report.csv\n");
    }

    /* 清理 */
    for (int p = 0; p < 6; p++) free(param_arrays[p]);
}
