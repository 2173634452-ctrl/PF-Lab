#include "backup.h"
#include "data_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

/* ═════════════════════════════════════════════════════════════════════
 *  1.7 数据备份
 * ═════════════════════════════════════════════════════════════════════ */

bool backup_dataset(const char *directory, const WaterDataset *dataset) {
    if (!dataset || !dataset->records) {
        printf("备份失败：无数据可备份。\n");
        return false;
    }

    /* 生成带时间戳的文件名 backup_YYYYMMDD_HHMMSS.csv */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char filename[512];
    snprintf(filename, sizeof(filename),
             "%s/backup_%04d%02d%02d_%02d%02d%02d.csv",
             directory,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);

    /* 确保备份目录存在（尝试创建） */
#ifdef _WIN32
    mkdir(directory);
#else
    mkdir(directory, 0755);
#endif

    /* 写入 CSV 格式备份 */
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("备份失败：无法创建文件 \"%s\"\n", filename);
        return false;
    }

    /* 备份时包含所有记录（有效+无效），以保留完整原始信息 */
    fprintf(fp, "Timestamp,Temp,Salinity,pH,DO,Precipitation,Air_temp\n");

    for (size_t i = 0; i < dataset->total_count; i++) {
        const WaterRecord *r = &dataset->records[i];
        if (r->valid) {
            fprintf(fp, "%s,%.2f,%.2f,%.2f,%.2f,%.4f,%.2f\n",
                    r->timestamp, r->temp, r->salinity, r->pH,
                    r->DO, r->precipitation, r->air_temp);
        } else {
            /* 无效记录以 NaN 占位 */
            fprintf(fp, "%s,NaN,NaN,NaN,NaN,NaN,NaN\n", r->timestamp);
        }
    }

    fclose(fp);
    printf("备份成功，文件已保存至 %s\n", filename);
    return true;
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.7 列出备份文件
 * ═════════════════════════════════════════════════════════════════════ */

int list_backup_files(const char *directory, char filenames[][512], int max_count) {
    int count = 0;

#ifdef _WIN32
    /* Windows: 使用 FindFirstFile / FindNextFile */
    WIN32_FIND_DATA find_data;
    HANDLE hFind;
    char search_path[512];
    snprintf(search_path, sizeof(search_path), "%s\\backup_*.csv", directory);
    hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (count < max_count) {
            snprintf(filenames[count], 512, "%s/%s", directory, find_data.cFileName);
            count++;
        }
    } while (FindNextFile(hFind, &find_data) && count < max_count);
    FindClose(hFind);
#else
    /* POSIX: 使用 opendir / readdir */
    DIR *dir = opendir(directory);
    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        if (strncmp(entry->d_name, "backup_", 7) == 0
            && strstr(entry->d_name, ".csv")) {
            snprintf(filenames[count], 512, "%s/%s", directory, entry->d_name);
            count++;
        }
    }
    closedir(dir);
#endif

    return count;
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.7 验证备份文件格式
 * ═════════════════════════════════════════════════════════════════════ */

bool validate_backup_file(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        printf("验证失败：无法打开文件 \"%s\"\n", filepath);
        return false;
    }

    char line[2048];
    int line_count = 0;
    bool has_header = false;

    while (fgets(line, sizeof(line), fp)) {
        line_count++;

        /* 检查第一行是否为表头 */
        if (line_count == 1) {
            if (strstr(line, "Timestamp") && strstr(line, "Temp")) {
                has_header = true;
                continue;
            }
        }

        /* 检查字段数（逗号计数，应有 6 个逗号 → 7 个字段） */
        int commas = 0;
        for (char *p = line; *p; p++) {
            if (*p == ',') commas++;
        }
        if (commas != 6) {
            printf("验证失败：第 %d 行字段数异常（%d 个逗号，预期 6 个）。\n",
                   line_count, commas);
            fclose(fp);
            return false;
        }
    }

    fclose(fp);

    int data_lines = has_header ? line_count - 1 : line_count;
    if (data_lines <= 0) {
        printf("验证失败：备份文件无数据行。\n");
        return false;
    }

    printf("格式验证通过：%d 行数据，7 列。\n", data_lines);
    return true;
}


/* ═════════════════════════════════════════════════════════════════════
 *  1.7 数据恢复
 * ═════════════════════════════════════════════════════════════════════ */

WaterDataset *restore_dataset(const char *filepath) {
    if (!filepath || strlen(filepath) == 0) {
        printf("恢复失败：未指定文件路径。\n");
        return NULL;
    }

    /* 先验证格式 */
    if (!validate_backup_file(filepath)) {
        printf("恢复中止：备份文件格式验证未通过。\n");
        return NULL;
    }

    /* 验证通过，调用数据加载模块读取备份 */
    WaterDataset *dataset = load_csv_data(filepath);
    if (dataset) {
        printf("恢复成功！从 %s 加载了 %zu 条记录（有效 %zu 条）。\n",
               filepath, dataset->total_count, dataset->valid_count);
    } else {
        printf("恢复失败：无法从备份文件加载数据。\n");
    }

    return dataset;
}
