#include "data_io.h"
#include "utils.h"
#include "backup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ═════════════════════════════════════════════════════════════════════
 *  内部辅助函数
 * ═════════════════════════════════════════════════════════════════════ */

/* 判断字段是否为缺失值标记：空串 / NaN / -999 / -9999 */
static bool is_missing_value(const char *token) {
    if (token == NULL || strlen(token) == 0) return true;
    if (strcmp(token, "NaN") == 0 || strcmp(token, "nan") == 0) return true;
    if (strcmp(token, "NAN") == 0) return true;
    if (strcmp(token, "-999")  == 0 || strcmp(token, "-999.0")  == 0) return true;
    if (strcmp(token, "-9999") == 0 || strcmp(token, "-9999.0") == 0) return true;
    return false;
}

/* 安全地将字段转换为 double；失败或缺失值返回 false */
static bool parse_double_field(const char *token, double *value) {
    if (is_missing_value(token)) return false;
    char *endptr = NULL;
    *value = strtod(token, &endptr);
    if (endptr == token) return false;  /* 完全无法解析 */
    return true;
}

/* 按分隔符分割一行 CSV，返回字段数 */
static int split_csv_line(char *line, char *fields[], int max_fields) {
    int count = 0;
    char *p = line;
    fields[count++] = p;
    while (*p) {
        if (*p == ',') {
            *p = '\0';
            if (count < max_fields) {
                fields[count++] = p + 1;
            }
        }
        p++;
    }
    /* 去除行尾换行符（兼容 \r\n、\n、\r 等各种组合） */
    while (p > line && (*(p - 1) == '\n' || *(p - 1) == '\r')) {
        *(--p) = '\0';
    }
    return count;
}

/* 根据参数名获取 double 指针，用于排序/筛选 */
static double *get_param_ptr(WaterRecord *rec, const char *param_name) {
    if (strcmp(param_name, "temp") == 0 || strcmp(param_name, "水温") == 0)
        return &rec->temp;
    if (strcmp(param_name, "salinity") == 0 || strcmp(param_name, "盐度") == 0)
        return &rec->salinity;
    if (strcmp(param_name, "pH") == 0 || strcmp(param_name, "ph") == 0)
        return &rec->pH;
    if (strcmp(param_name, "DO") == 0 || strcmp(param_name, "do") == 0
        || strcmp(param_name, "溶解氧") == 0)
        return &rec->DO;
    if (strcmp(param_name, "precipitation") == 0 || strcmp(param_name, "降水量") == 0)
        return &rec->precipitation;
    if (strcmp(param_name, "air_temp") == 0 || strcmp(param_name, "气温") == 0)
        return &rec->air_temp;
    return NULL;
}

/* 获取参数合理范围，返回 true 表示在范围内 */
static bool validate_param_range(const char *param_name, double value) {
    if (strcmp(param_name, "temp") == 0 || strcmp(param_name, "水温") == 0)
        return value >= VALID_TEMP_MIN && value <= VALID_TEMP_MAX;
    if (strcmp(param_name, "salinity") == 0 || strcmp(param_name, "盐度") == 0)
        return value >= VALID_SALINITY_MIN && value <= VALID_SALINITY_MAX;
    if (strcmp(param_name, "pH") == 0 || strcmp(param_name, "ph") == 0)
        return value >= VALID_PH_MIN && value <= VALID_PH_MAX;
    if (strcmp(param_name, "DO") == 0 || strcmp(param_name, "do") == 0
        || strcmp(param_name, "溶解氧") == 0)
        return value >= VALID_DO_MIN && value <= VALID_DO_MAX;
    if (strcmp(param_name, "precipitation") == 0 || strcmp(param_name, "降水量") == 0)
        return value >= VALID_PRECIP_MIN && value <= VALID_PRECIP_MAX;
    if (strcmp(param_name, "air_temp") == 0 || strcmp(param_name, "气温") == 0)
        return value >= VALID_AIR_TEMP_MIN && value <= VALID_AIR_TEMP_MAX;
    return false;
}

/* 获取参数合理范围的最小/最大值 */
static void get_param_range(const char *param_name, double *min, double *max) {
    if (strcmp(param_name, "temp") == 0 || strcmp(param_name, "水温") == 0)
        { *min = VALID_TEMP_MIN; *max = VALID_TEMP_MAX; }
    else if (strcmp(param_name, "salinity") == 0 || strcmp(param_name, "盐度") == 0)
        { *min = VALID_SALINITY_MIN; *max = VALID_SALINITY_MAX; }
    else if (strcmp(param_name, "pH") == 0 || strcmp(param_name, "ph") == 0)
        { *min = VALID_PH_MIN; *max = VALID_PH_MAX; }
    else if (strcmp(param_name, "DO") == 0 || strcmp(param_name, "do") == 0
             || strcmp(param_name, "溶解氧") == 0)
        { *min = VALID_DO_MIN; *max = VALID_DO_MAX; }
    else if (strcmp(param_name, "precipitation") == 0 || strcmp(param_name, "降水量") == 0)
        { *min = VALID_PRECIP_MIN; *max = VALID_PRECIP_MAX; }
    else if (strcmp(param_name, "air_temp") == 0 || strcmp(param_name, "气温") == 0)
        { *min = VALID_AIR_TEMP_MIN; *max = VALID_AIR_TEMP_MAX; }
    else
        { *min = 0; *max = 0; }
}

/* 打印参数合理范围 */
static void print_param_ranges(void) {
    printf("  水温(Temp):        [%.1f, %.1f] ℃\n", VALID_TEMP_MIN, VALID_TEMP_MAX);
    printf("  盐度(Salinity):    [%.0f, %.0f] PSU\n", VALID_SALINITY_MIN, VALID_SALINITY_MAX);
    printf("  pH:                [%.1f, %.1f]\n", VALID_PH_MIN, VALID_PH_MAX);
    printf("  溶解氧(DO):        [%.0f, %.0f] mg/L\n", VALID_DO_MIN, VALID_DO_MAX);
    printf("  降水量(precip):    [%.0f, %.0f] m\n", VALID_PRECIP_MIN, VALID_PRECIP_MAX);
    printf("  气温(Air_temp):    [%.0f, %.0f] ℃\n", VALID_AIR_TEMP_MIN, VALID_AIR_TEMP_MAX);
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.2 文件读取
 * ═════════════════════════════════════════════════════════════════════ */

WaterDataset *load_csv_data(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("错误：无法打开文件 \"%s\"，请确认文件是否存在。\n", filename);
        return NULL;
    }

    WaterDataset *dataset = (WaterDataset *)malloc(sizeof(WaterDataset));
    if (!dataset) {
        printf("错误：内存不足，无法创建数据集。\n");
        fclose(fp);
        return NULL;
    }

    dataset->capacity = INITIAL_CAPACITY;
    dataset->records = (WaterRecord *)malloc(sizeof(WaterRecord) * dataset->capacity);
    if (!dataset->records) {
        printf("错误：内存不足，无法分配记录数组。\n");
        free(dataset);
        fclose(fp);
        return NULL;
    }

    dataset->total_count = 0;
    dataset->valid_count = 0;
    dataset->preprocessed = false;  /* 新加载的数据尚未预处理 */

    char line[2048];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* 跳过第一行（表头） */
        if (line_num == 1) {
            continue;
        }

        /* 跳过空行 */
        int len = (int)strlen(line);
        if (len <= 1) continue;

        /* 动态扩容：容量满时翻倍 */
        if (dataset->total_count >= dataset->capacity) {
            size_t new_capacity = dataset->capacity * 2;
            WaterRecord *tmp = (WaterRecord *)realloc(
                dataset->records, sizeof(WaterRecord) * new_capacity);
            if (!tmp) {
                printf("错误：内存不足，扩容失败（已读取 %zu 条）。\n",
                       dataset->total_count);
                free_dataset(dataset);
                fclose(fp);
                return NULL;
            }
            dataset->records = tmp;
            dataset->capacity = new_capacity;
            printf("  [系统] 动态扩容：容量 %zu → %zu\n",
                   dataset->capacity / 2, dataset->capacity);
        }

        /* 初始化记录 */
        WaterRecord *rec = &dataset->records[dataset->total_count];
        memset(rec, 0, sizeof(WaterRecord));
        rec->valid = true;  /* 默认有效，遇到问题再标记 */

        /* 手动分割 CSV 行 */
        char *fields[7] = {NULL};
        int field_count = split_csv_line(line, fields, 7);

        if (field_count < 6) {
            /* 字段数不足 → 标记无效 */
            rec->valid = false;
            dataset->total_count++;
            continue;
        }

        /* 时间戳 */
        if (field_count >= 7) {
            /* 7列：第0列是时间戳，其余为数据 */
            strncpy(rec->timestamp, fields[0], sizeof(rec->timestamp) - 1);
            rec->timestamp[sizeof(rec->timestamp) - 1] = '\0';
        } else {
            /* 6列：无时间戳列，按5分钟间隔自动生成（从2025-01-01 12:00:00开始，符合任务书要求） */
            int total_minutes = (int)dataset->total_count * 5;
            struct tm base = {0};
            base.tm_year = 2025 - 1900;  /* 2025年 */
            base.tm_mon  = 0;            /* January (0-based) */
            base.tm_mday = 1;
            base.tm_hour = 12;           /* 12:00 起始 */
            base.tm_min  = 0;
            base.tm_sec  = 0;
            time_t t = mktime(&base) + total_minutes * 60;
            struct tm *local = localtime(&t);
            strftime(rec->timestamp, sizeof(rec->timestamp),
                     "%Y-%m-%d %H:%M:%S", local);
        }

        /* 解析各数值字段：无时间戳时字段偏移量为0，有时间戳时偏移量为1 */
        int offset = (field_count >= 7) ? 1 : 0;
        if (!parse_double_field(fields[offset + 0], &rec->temp))
            { rec->valid = false; rec->temp = NAN; }
        if (!parse_double_field(fields[offset + 1], &rec->salinity))
            { rec->valid = false; rec->salinity = NAN; }
        if (!parse_double_field(fields[offset + 2], &rec->pH))
            { rec->valid = false; rec->pH = NAN; }
        if (!parse_double_field(fields[offset + 3], &rec->DO))
            { rec->valid = false; rec->DO = NAN; }
        if (!parse_double_field(fields[offset + 4], &rec->precipitation))
            { rec->valid = false; rec->precipitation = NAN; }
        if (!parse_double_field(fields[offset + 5], &rec->air_temp))
            { rec->valid = false; rec->air_temp = NAN; }

        if (rec->valid) dataset->valid_count++;
        dataset->total_count++;
    }

    fclose(fp);
    printf("读取完成：总记录 %zu，有效记录 %zu（%.1f%%）\n",
           dataset->total_count, dataset->valid_count,
           dataset->total_count > 0
               ? 100.0 * dataset->valid_count / dataset->total_count
               : 0.0);

    /* 写入数据概览文件 */
    write_data_overview(dataset);

    return dataset;
}

void write_data_overview(const WaterDataset *dataset) {
    if (!dataset) return;

    /* 确保 reports 目录存在（简单尝试） */
    FILE *fp = fopen("reports/data_overview.txt", "w");
    if (!fp) {
        printf("警告：无法写入数据概览文件 reports/data_overview.txt\n");
        return;
    }

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    /* 去除 ctime 自带换行 */
    time_str[strcspn(time_str, "\n")] = '\0';

    fprintf(fp, "╔══════════════════════════════════════╗\n");
    fprintf(fp, "║     海水养殖水质数据概览报告         ║\n");
    fprintf(fp, "╚══════════════════════════════════════╝\n\n");
    fprintf(fp, "生成时间: %s\n\n", time_str);
    fprintf(fp, "总记录数:         %zu\n", dataset->total_count);
    fprintf(fp, "有效记录数:       %zu\n", dataset->valid_count);
    fprintf(fp, "数据有效率:       %.2f%%\n",
            dataset->total_count > 0
                ? 100.0 * dataset->valid_count / dataset->total_count
                : 0.0);
    fprintf(fp, "当前容量:         %zu\n", dataset->capacity);

    fclose(fp);
    printf("数据概览已写入 reports/data_overview.txt\n");
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.3 数据存储
 * ═════════════════════════════════════════════════════════════════════ */

bool save_csv_data(const char *filename, const WaterDataset *dataset) {
    if (!dataset || !dataset->records) return false;

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("错误：无法创建文件 \"%s\"\n", filename);
        return false;
    }

    /* 写入表头 */
    fprintf(fp, "Timestamp,Temp,Salinity,pH,DO,Precipitation,Air_temp\n");

    for (size_t i = 0; i < dataset->total_count; i++) {
        const WaterRecord *rec = &dataset->records[i];
        if (rec->valid) {
            fprintf(fp, "%s,%.2f,%.2f,%.2f,%.2f,%.4f,%.2f\n",
                    rec->timestamp,
                    rec->temp,
                    rec->salinity,
                    rec->pH,
                    rec->DO,
                    rec->precipitation,
                    rec->air_temp);
        } else {
            /* 无效记录以 NaN 占位，与备份保持一致 */
            fprintf(fp, "%s,NaN,NaN,NaN,NaN,NaN,NaN\n", rec->timestamp);
        }
    }

    fclose(fp);
    return true;
}

bool save_binary_data(const char *filename, const WaterDataset *dataset) {
    if (!dataset || !dataset->records) return false;

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("错误：无法创建二进制文件 \"%s\"\n", filename);
        return false;
    }

    /* 先写元信息：记录数、有效数、容量 */
    fwrite(&dataset->total_count, sizeof(size_t), 1, fp);
    fwrite(&dataset->valid_count, sizeof(size_t), 1, fp);
    fwrite(&dataset->capacity,    sizeof(size_t), 1, fp);

    /* 整块写入所有记录 */
    size_t written = fwrite(dataset->records,
                            sizeof(WaterRecord), dataset->total_count, fp);
    fclose(fp);

    if (written != dataset->total_count) {
        printf("警告：写入记录数不匹配 (%zu / %zu)\n",
               written, dataset->total_count);
        return false;
    }
    return true;
}

WaterDataset *load_binary_data(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("错误：无法打开二进制文件 \"%s\"\n", filename);
        return NULL;
    }

    WaterDataset *dataset = (WaterDataset *)malloc(sizeof(WaterDataset));
    if (!dataset) {
        printf("错误：内存不足\n");
        fclose(fp);
        return NULL;
    }

    /* 读取元信息 */
    size_t n1 = fread(&dataset->total_count, sizeof(size_t), 1, fp);
    size_t n2 = fread(&dataset->valid_count, sizeof(size_t), 1, fp);
    size_t n3 = fread(&dataset->capacity,    sizeof(size_t), 1, fp);

    if (n1 != 1 || n2 != 1 || n3 != 1) {
        printf("错误：二进制文件格式损坏\n");
        free(dataset);
        fclose(fp);
        return NULL;
    }

    dataset->records = (WaterRecord *)malloc(
        sizeof(WaterRecord) * dataset->capacity);
    if (!dataset->records) {
        printf("错误：内存不足\n");
        free(dataset);
        fclose(fp);
        return NULL;
    }

    size_t read_count = fread(dataset->records,
                              sizeof(WaterRecord), dataset->total_count, fp);
    fclose(fp);

    if (read_count != dataset->total_count) {
        printf("警告：读取记录数不匹配 (%zu / %zu)\n",
               read_count, dataset->total_count);
    }

    printf("二进制数据加载完成：总记录 %zu，有效 %zu\n",
           dataset->total_count, dataset->valid_count);
    return dataset;
}

void compare_storage_performance(const WaterDataset *dataset) {
    if (!dataset) {
        printf("错误：无数据集，请先加载数据。\n");
        return;
    }

    const char *csv_file    = "data/processed/perf_test.csv";
    const char *binary_file = "data/processed/perf_test.bin";

    clock_t start, end;
    double csv_write_time = 0, csv_read_time = 0;
    double bin_write_time = 0, bin_read_time = 0;

    /* ── CSV 写入 ── */
    start = clock();
    bool csv_ok = save_csv_data(csv_file, dataset);
    end = clock();
    csv_write_time = csv_ok ? (double)(end - start) / CLOCKS_PER_SEC : -1;

    /* ── CSV 读取 ── */
    if (csv_ok) {
        start = clock();
        WaterDataset *csv_ds = load_csv_data(csv_file);
        end = clock();
        csv_read_time = (double)(end - start) / CLOCKS_PER_SEC;
        if (csv_ds) free_dataset(csv_ds);
    }

    /* ── 二进制写入 ── */
    start = clock();
    bool bin_ok = save_binary_data(binary_file, dataset);
    end = clock();
    bin_write_time = bin_ok ? (double)(end - start) / CLOCKS_PER_SEC : -1;

    /* ── 二进制读取 ── */
    if (bin_ok) {
        start = clock();
        WaterDataset *bin_ds = load_binary_data(binary_file);
        end = clock();
        bin_read_time = (double)(end - start) / CLOCKS_PER_SEC;
        if (bin_ds) free_dataset(bin_ds);
    }

    /* ── 获取文件大小 ── */
    long csv_size = 0, bin_size = 0;
    {
        FILE *f = fopen(csv_file, "rb");
        if (f) { fseek(f, 0, SEEK_END); csv_size = ftell(f); fclose(f); }
        f = fopen(binary_file, "rb");
        if (f) { fseek(f, 0, SEEK_END); bin_size = ftell(f); fclose(f); }
    }

    /* ── 理论大小 ── */
    size_t theory_csv  = dataset->valid_count * 50;  /* 估算每行约50字节 */
    size_t theory_bin  = dataset->total_count * sizeof(WaterRecord)
                         + 3 * sizeof(size_t);

    /* ── 打印对比表 ── */
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              存储格式性能对比（1.3）                         ║\n");
    printf("╠═══════════╤══════════════╤════════════╤═══════════╤════════╣\n");
    printf("║ 存储格式  │ 文件大小(B)  │ 写入时间(s) │ 读取时间(s)│ 可读  ║\n");
    printf("╟───────────┼──────────────┼────────────┼───────────┼────────╢\n");
    printf("║ CSV文本   │ %-12ld │ %-10.4f │ %-9.4f │  是   ║\n",
           csv_size, csv_write_time, csv_read_time);
    printf("║ 二进制    │ %-12ld │ %-10.4f │ %-9.4f │  否   ║\n",
           bin_size, bin_write_time, bin_read_time);
    printf("╚═══════════╧══════════════╧════════════╧═══════════╧════════╝\n");
    printf("\n  理论原始数据大小 ≈ %zu 字节（CSV估算）/ %zu 字节（二进制）\n",
           theory_csv, theory_bin);

    /* 清理测试文件 */
    remove(csv_file);
    remove(binary_file);
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.4 数据查询
 * ═════════════════════════════════════════════════════════════════════ */

void view_data_paginated(const WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("无数据可显示。\n");
        return;
    }

    size_t total_pages = (dataset->total_count + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t current_page = 1;

    while (1) {
        printf("\n╔══════════════════════════════════════════════════════════════╗\n");
        printf("║  数据浏览 — 第 %zu / %zu 页（共 %zu 条记录）                  ║\n",
               current_page, total_pages, dataset->total_count);
        printf("╠══════════════════════════════════════════════════════════════╣\n");
        printf("║ %-4s %-20s %6s %6s %6s %6s %6s %6s ║\n",
               "序号", "时间", "水温", "盐度", "pH", "DO", "降水", "气温");
        printf("╠══════════════════════════════════════════════════════════════╣\n");

        size_t start = (current_page - 1) * PAGE_SIZE;
        size_t end   = start + PAGE_SIZE;
        if (end > dataset->total_count) end = dataset->total_count;

        for (size_t i = start; i < end; i++) {
            const WaterRecord *r = &dataset->records[i];
            printf("║ %-4zu %-20s %6.1f %6.1f %6.2f %6.2f %6.3f %6.1f %s║\n",
                   i + 1, r->timestamp,
                   r->temp, r->salinity, r->pH, r->DO,
                   r->precipitation, r->air_temp,
                   r->valid ? " " : "*");
        }
        printf("╚══════════════════════════════════════════════════════════════╝\n");
        printf("  (* 标记表示该记录含缺失/无效数据)\n");
        printf("  [N]下一页  [P]上一页  [J]跳转  [Q]返回: ");

        char cmd[16];
        scanf("%15s", cmd);
        while (getchar() != '\n'); /* 清缓冲 */

        if (cmd[0] == 'N' || cmd[0] == 'n') {
            if (current_page < total_pages) current_page++;
            else printf("已是最后一页。\n");
        } else if (cmd[0] == 'P' || cmd[0] == 'p') {
            if (current_page > 1) current_page--;
            else printf("已是第一页。\n");
        } else if (cmd[0] == 'J' || cmd[0] == 'j') {
            printf("跳转到第几页？(1-%zu): ", total_pages);
            int target;
            if (scanf("%d", &target) == 1) {
                while (getchar() != '\n');
                if (target >= 1 && (size_t)target <= total_pages)
                    current_page = (size_t)target;
                else
                    printf("页码超出范围。\n");
            } else {
                while (getchar() != '\n');
                printf("输入无效。\n");
            }
        } else if (cmd[0] == 'Q' || cmd[0] == 'q') {
            break;
        } else {
            printf("无效命令。\n");
        }
    }
}

void filter_data_by_range(const WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("无数据可筛选。\n");
        return;
    }

    printf("\n可筛选参数: temp / salinity / pH / DO / precipitation / air_temp\n");
    printf("中文别名: 水温 / 盐度 / 溶解氧 / 降水量 / 气温\n");
    printf("请输入参数名: ");
    char param[32];
    scanf("%31s", param);
    while (getchar() != '\n');

    double min_val, max_val;
    printf("请输入最小值: ");
    if (scanf("%lf", &min_val) != 1) {
        printf("输入无效。\n");
        while (getchar() != '\n');
        return;
    }
    printf("请输入最大值: ");
    if (scanf("%lf", &max_val) != 1) {
        printf("输入无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  筛选结果: %s ∈ [%.2f, %.2f]                                  ║\n",
           param, min_val, max_val);
    printf("╠══════╤══════════════════════╤════════════════════════════════╣\n");

    int found = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        const WaterRecord *r = &dataset->records[i];
        if (!r->valid) continue;

        double *val = get_param_ptr((WaterRecord *)r, param);
        if (!val) {
            printf("未知参数: %s\n", param);
            return;
        }

        if (*val >= min_val && *val <= max_val) {
            printf("║ %4zu │ %-20s │ %s=%-8.2f               ║\n",
                   i + 1, r->timestamp, param, *val);
            found++;
            if (found >= 100) {
                printf("║ ... (已显示前100条，共匹配更多记录)                          ║\n");
                break;
            }
        }
    }
    printf("╚══════╧══════════════════════╧════════════════════════════════╝\n");
    printf("共匹配 %d 条记录。\n", found);
}

/* qsort 用全局变量传递排序参数（简化设计） */
static int g_sort_param_idx = 0;
static int g_sort_direction = 1;  /* 1=升序, -1=降序 */

static int compare_records(const void *a, const void *b) {
    const WaterRecord *ra = (const WaterRecord *)a;
    const WaterRecord *rb = (const WaterRecord *)b;

    double va = 0, vb = 0;
    switch (g_sort_param_idx) {
        case 0: va = ra->temp;         vb = rb->temp;         break;
        case 1: va = ra->salinity;     vb = rb->salinity;     break;
        case 2: va = ra->pH;           vb = rb->pH;           break;
        case 3: va = ra->DO;           vb = rb->DO;           break;
        case 4: va = ra->precipitation; vb = rb->precipitation; break;
        case 5: va = ra->air_temp;     vb = rb->air_temp;     break;
        default: return 0;
    }

    if (va < vb) return -1 * g_sort_direction;
    if (va > vb) return  1 * g_sort_direction;
    return 0;
}

void sort_and_display_data(WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("无数据可排序。\n");
        return;
    }

    printf("\n可排序参数:\n");
    printf("  [0] 水温(Temp)     [1] 盐度(Salinity)\n");
    printf("  [2] pH             [3] 溶解氧(DO)\n");
    printf("  [4] 降水量(Precip) [5] 气温(Air_temp)\n");
    printf("请选择参数编号 (0-5): ");

    int idx;
    if (scanf("%d", &idx) != 1 || idx < 0 || idx > 5) {
        printf("输入无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    printf("排序方向: [A]升序  [D]降序: ");
    char dir;
    scanf("%c", &dir);
    while (getchar() != '\n');

    int direction = (dir == 'D' || dir == 'd') ? -1 : 1;

    g_sort_param_idx = idx;
    g_sort_direction  = direction;

    /* 复制一份排序（不改变原始顺序，仅展示） */
    size_t n = dataset->total_count;
    WaterRecord *sorted = (WaterRecord *)malloc(sizeof(WaterRecord) * n);
    if (!sorted) {
        printf("内存不足。\n");
        return;
    }
    memcpy(sorted, dataset->records, sizeof(WaterRecord) * n);

    qsort(sorted, n, sizeof(WaterRecord), compare_records);

    const char *param_names[] = {"水温", "盐度", "pH", "溶解氧", "降水量", "气温"};
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  按 %s %s排序（前50条）                                       ║\n",
           param_names[idx], direction == 1 ? "升序" : "降序");
    printf("╠══════╤══════════════════════╤════════════════════════════════╣\n");

    size_t display_count = n < 50 ? n : 50;
    for (size_t i = 0; i < display_count; i++) {
        double val = 0;
        switch (idx) {
            case 0: val = sorted[i].temp;         break;
            case 1: val = sorted[i].salinity;     break;
            case 2: val = sorted[i].pH;           break;
            case 3: val = sorted[i].DO;           break;
            case 4: val = sorted[i].precipitation; break;
            case 5: val = sorted[i].air_temp;     break;
        }
        printf("║ %4zu │ %-20s │ %s=%-8.2f                    ║\n",
               i + 1, sorted[i].timestamp, param_names[idx], val);
    }
    printf("╚══════╧══════════════════════╧════════════════════════════════╝\n");

    free(sorted);
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.5 数据修改
 * ═════════════════════════════════════════════════════════════════════ */

void modify_single_record(WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("无数据可修改。\n");
        return;
    }

    printf("请输入要修改的记录号 (1-%zu): ", dataset->total_count);
    size_t record_no;
    if (scanf("%zu", &record_no) != 1
        || record_no < 1 || record_no > dataset->total_count) {
        printf("记录号无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    WaterRecord *rec = &dataset->records[record_no - 1];

    printf("\n当前记录 #%zu:\n", record_no);
    printf("  时间: %s\n", rec->timestamp);
    printf("  [1] 水温(Temp):      %.2f ℃\n", rec->temp);
    printf("  [2] 盐度(Salinity):  %.2f PSU\n", rec->salinity);
    printf("  [3] pH:              %.2f\n", rec->pH);
    printf("  [4] 溶解氧(DO):      %.2f mg/L\n", rec->DO);
    printf("  [5] 降水量(Precip):  %.4f m\n", rec->precipitation);
    printf("  [6] 气温(Air_temp):  %.2f ℃\n", rec->air_temp);

    printf("\n各参数合理范围:\n");
    print_param_ranges();

    printf("\n请选择要修改的参数 (1-6): ");
    int param_choice;
    if (scanf("%d", &param_choice) != 1 || param_choice < 1 || param_choice > 6) {
        printf("选择无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    printf("请输入新值: ");
    double new_value;
    if (scanf("%lf", &new_value) != 1) {
        printf("输入无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    /* 验证范围 */
    const char *param_names[] = {"temp", "salinity", "pH", "DO",
                                  "precipitation", "air_temp"};
    if (!validate_param_range(param_names[param_choice - 1], new_value)) {
        double min_v, max_v;
        get_param_range(param_names[param_choice - 1], &min_v, &max_v);
        printf("错误：新值 %.4f 不在合理范围 [%.2f, %.2f] 内，修改取消。\n",
               new_value, min_v, max_v);
        return;
    }

    /* 修改前自动备份 */
    printf("修改前自动备份原始数据...\n");
    backup_dataset("backup", dataset);

    /* 执行修改 */
    double *target = get_param_ptr(rec, param_names[param_choice - 1]);
    *target = new_value;
    rec->valid = true;  /* 修改后标记为有效 */

    printf("修改成功！\n");
    dataset->preprocessed = false;  /* 数据被修改，预处理标记失效 */

    /* 询问是否保存 */
    if (confirm_action("是否将修改后的数据保存到文件？")) {
        printf("保存到 data/processed/modified_data.csv ...\n");
        if (save_csv_data("data/processed/modified_data.csv", dataset)) {
            printf("保存成功！\n");
        }
    }
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.6 数据删除
 * ═════════════════════════════════════════════════════════════════════ */

void delete_single_record(WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("无数据可删除。\n");
        return;
    }

    printf("请输入要删除的记录号 (1-%zu): ", dataset->total_count);
    size_t record_no;
    if (scanf("%zu", &record_no) != 1
        || record_no < 1 || record_no > dataset->total_count) {
        printf("记录号无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    /* 显示待删除记录 */
    WaterRecord *rec = &dataset->records[record_no - 1];
    printf("\n待删除记录 #%zu:\n", record_no);
    printf("  时间: %s | 水温:%.1f 盐度:%.1f pH:%.2f DO:%.2f 降水:%.3f 气温:%.1f\n",
           rec->timestamp, rec->temp, rec->salinity, rec->pH,
           rec->DO, rec->precipitation, rec->air_temp);

    /* 确认删除 */
    if (!confirm_action("确认删除该记录？此操作不可撤销！")) {
        printf("删除已取消。\n");
        return;
    }

    /* 删除前自动备份 */
    printf("删除前自动备份原始数据...\n");
    backup_dataset("backup", dataset);

    /* 执行删除（将后续记录前移） */
    for (size_t i = record_no - 1; i < dataset->total_count - 1; i++) {
        dataset->records[i] = dataset->records[i + 1];
    }
    dataset->total_count--;
    /* 更新有效计数 */
    dataset->valid_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) dataset->valid_count++;
    }

    printf("删除成功！当前总记录数: %zu\n", dataset->total_count);
    dataset->preprocessed = false;  /* 数据被删除，预处理标记失效 */
    write_data_overview(dataset);
}

void batch_delete_records(WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("无数据可删除。\n");
        return;
    }

    printf("\n批量删除 — 按条件筛选要删除的记录\n");
    printf("可筛选参数: temp / salinity / pH / DO / precipitation / air_temp\n");
    printf("请输入参数名: ");
    char param[32];
    scanf("%31s", param);
    while (getchar() != '\n');

    double *test_ptr = get_param_ptr(&dataset->records[0], param);
    if (!test_ptr) {
        printf("未知参数: %s\n", param);
        return;
    }

    double min_val, max_val;
    printf("请输入最小值: ");
    if (scanf("%lf", &min_val) != 1) {
        printf("输入无效。\n");
        while (getchar() != '\n');
        return;
    }
    printf("请输入最大值: ");
    if (scanf("%lf", &max_val) != 1) {
        printf("输入无效。\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    /* 统计匹配数 */
    size_t match_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        double *val = get_param_ptr(&dataset->records[i], param);
        if (*val >= min_val && *val <= max_val) match_count++;
    }

    if (match_count == 0) {
        printf("未找到匹配 %s ∈ [%.2f, %.2f] 的记录。\n", param, min_val, max_val);
        return;
    }

    printf("找到 %zu 条匹配记录。\n", match_count);

    if (!confirm_action("确认批量删除这些记录？此操作不可撤销！")) {
        printf("删除已取消。\n");
        return;
    }

    /* 删除前自动备份 */
    printf("删除前自动备份原始数据...\n");
    backup_dataset("backup", dataset);

    /* 执行批量删除（保留不匹配的记录） */
    size_t write_pos = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        double *val = get_param_ptr(&dataset->records[i], param);
        if (*val >= min_val && *val <= max_val) {
            /* 匹配 → 跳过（不保留） */
            continue;
        }
        if (write_pos != i) {
            dataset->records[write_pos] = dataset->records[i];
        }
        write_pos++;
    }
    dataset->total_count = write_pos;

    /* 更新有效计数 */
    dataset->valid_count = 0;
    for (size_t i = 0; i < dataset->total_count; i++) {
        if (dataset->records[i].valid) dataset->valid_count++;
    }

    printf("批量删除成功！删除了 %zu 条记录，当前总记录数: %zu\n",
           match_count, dataset->total_count);
    dataset->preprocessed = false;  /* 数据被删除，预处理标记失效 */
    write_data_overview(dataset);
}


/* ═════════════════════════════════════════════════════════════════════
 *  通用
 * ═════════════════════════════════════════════════════════════════════ */

void free_dataset(WaterDataset *dataset) {
    if (!dataset) return;
    free(dataset->records);
    free(dataset);
}
