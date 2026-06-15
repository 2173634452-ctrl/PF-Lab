#include "ui.h"
#include "utils.h"
#include "data_io.h"
#include "backup.h"
#include "preprocess.h"
#include "analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ═════════════════════════════════════════════════════════════════════
 *  主菜单
 * ═════════════════════════════════════════════════════════════════════ */

void show_main_menu(void) {
    printf("\n========================================\n");
    printf("  海水养殖水质分析系统 v1.0\n");
    printf("========================================\n");
    printf(" [1] 数据基础操作\n");
    printf(" [2] 数据预处理\n");
    printf(" [3] 统计分析\n");
    printf(" [4] 预测分析\n");
    printf(" [5] 查看数据概览\n");
    printf(" [6] 查看预警报告\n");
    printf(" [7] 查看分析报告\n");
    printf(" [8] 数据备份与恢复\n");
    printf(" [9] 清屏\n");
    printf(" [0] 退出系统\n");
    printf("========================================\n");
    printf("  请选择操作 (0-9): ");
}

void handle_menu_choice(int choice, WaterDataset **dataset) {
    switch (choice) {
        case 1: {
            /* 模块一子菜单循环 */
            int sub_choice = -1;
            while (sub_choice != 0) {
                show_data_submenu(*dataset);
                if (scanf("%d", &sub_choice) != 1) {
                    printf("输入错误，请重新输入。\n");
                    while (getchar() != '\n');
                    sub_choice = -1;
                    continue;
                }
                while (getchar() != '\n');
                handle_data_submenu(sub_choice, dataset);
            }
            break;
        }
        case 2: {
            /* 模块二子菜单循环 */
            int sub_choice = -1;
            while (sub_choice != 0) {
                show_preprocess_submenu(*dataset);
                if (scanf("%d", &sub_choice) != 1) {
                    printf("输入错误，请重新输入。\n");
                    while (getchar() != '\n');
                    sub_choice = -1;
                    continue;
                }
                while (getchar() != '\n');
                handle_preprocess_submenu(sub_choice, dataset);
            }
            break;
        }
        case 3: {
            /* 模块三子菜单循环 */
            int sub_choice = -1;
            while (sub_choice != 0) {
                show_analysis_submenu(*dataset);
                if (scanf("%d", &sub_choice) != 1) {
                    printf("输入错误，请重新输入。\n");
                    while (getchar() != '\n');
                    sub_choice = -1;
                    continue;
                }
                while (getchar() != '\n');
                handle_analysis_submenu(sub_choice, dataset);
            }
            break;
        }
        case 4:
            printf("进入预测分析模块...\n");
            break;
        case 5:
            display_overview(*dataset);
            break;
        case 6:
            printf("查看预警报告...\n");
            break;
        case 7:
            printf("查看分析报告...\n");
            break;
        case 8: {
            /* 主菜单快捷备份/恢复 */
            printf("  [1] 备份当前数据\n");
            printf("  [2] 从备份恢复\n");
            printf("  请选择: ");
            int bk_choice;
            if (scanf("%d", &bk_choice) != 1) {
                while (getchar() != '\n');
                break;
            }
            while (getchar() != '\n');
            if (bk_choice == 1) {
                if (*dataset) {
                    backup_dataset("backup", *dataset);
                } else {
                    printf("无数据可备份，请先加载数据。\n");
                }
            } else if (bk_choice == 2) {
                /* 列出备份并恢复 */
                char backups[64][512];
                int n = list_backup_files("backup", backups, 64);
                if (n <= 0) {
                    printf("没有可用的备份文件。\n");
                    break;
                }
                printf("\n可用的备份文件:\n");
                for (int i = 0; i < n; i++) {
                    printf("  [%d] %s\n", i + 1, backups[i]);
                }
                printf("请选择要恢复的备份编号 (1-%d): ", n);
                int sel;
                if (scanf("%d", &sel) == 1 && sel >= 1 && sel <= n) {
                    while (getchar() != '\n');
                    WaterDataset *restored = restore_dataset(backups[sel - 1]);
                    if (restored) {
                        if (*dataset) free_dataset(*dataset);
                        *dataset = restored;
                    }
                } else {
                    while (getchar() != '\n');
                    printf("选择无效。\n");
                }
            }
            break;
        }
        case 9:
            clear_console();
            break;
        case 0:
            /* 退出在主循环中处理 */
            break;
        default:
            printf("无效选择，请重新输入。\n");
            break;
    }
}


/* ═════════════════════════════════════════════════════════════════════
 *  模块一 子菜单
 * ═════════════════════════════════════════════════════════════════════ */

void show_data_submenu(const WaterDataset *dataset) {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║      模块一：数据基础操作          ║\n");
    printf("  ╠════════════════════════════════════╣\n");
    printf("  ║  [1] 加载CSV数据                   ║\n");
    printf("  ║  [2] 分页浏览数据                  ║\n");
    printf("  ║  [3] 按条件筛选                    ║\n");
    printf("  ║  [4] 按参数排序                    ║\n");
    printf("  ║  [5] 修改单条记录                  ║\n");
    printf("  ║  [6] 删除单条记录                  ║\n");
    printf("  ║  [7] 批量删除记录                  ║\n");
    printf("  ║  [8] 保存数据(CSV)                 ║\n");
    printf("  ║  [9] 保存数据(二进制)              ║\n");
    printf("  ║ [10] 存储性能对比                  ║\n");
    printf("  ║ [11] 手动备份数据                  ║\n");
    printf("  ║ [12] 从备份恢复数据                ║\n");
    printf("  ║  [0] 返回主菜单                    ║\n");
    printf("  ╚════════════════════════════════════╝\n");
    printf("  当前数据: %s\n",
           (dataset && dataset->total_count > 0) ? "已加载" : "未加载");
    printf("  请选择操作 (0-12): ");
}

/* 辅助：检查数据是否加载，未加载则提示 */
static bool ensure_data_loaded(const WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("提示：尚未加载数据，请先执行 [1] 加载CSV数据。\n");
        return false;
    }
    return true;
}

void handle_data_submenu(int choice, WaterDataset **dataset) {
    /* 子菜单中显示的当前数据状态 */
    switch (choice) {
        case 1: {
            /* 加载CSV数据 */
            char filename[256];
            printf("请输入CSV文件路径 (默认: data/raw/data.csv): ");
            if (scanf("%255s", filename) != 1) {
                while (getchar() != '\n');
                printf("输入无效。\n");
                break;
            }
            while (getchar() != '\n');

            /* 如果用户直接回车，使用默认路径 */
            if (strlen(filename) == 0) {
                strcpy(filename, "data/raw/data.csv");
            }

            WaterDataset *loaded = load_csv_data(filename);
            if (loaded) {
                /* 释放旧数据 */
                if (*dataset) {
                    free_dataset(*dataset);
                }
                *dataset = loaded;
                printf("数据加载成功！当前内存中有 %zu 条记录。\n",
                       loaded->total_count);
            }
            break;
        }
        case 2:
            /* 分页浏览 */
            if (ensure_data_loaded(*dataset)) {
                view_data_paginated(*dataset);
            }
            break;
        case 3:
            /* 按条件筛选 */
            if (ensure_data_loaded(*dataset)) {
                filter_data_by_range(*dataset);
            }
            break;
        case 4:
            /* 按参数排序 */
            if (ensure_data_loaded(*dataset)) {
                sort_and_display_data(*dataset);
            }
            break;
        case 5:
            /* 修改单条记录 */
            if (ensure_data_loaded(*dataset)) {
                modify_single_record(*dataset);
            }
            break;
        case 6:
            /* 删除单条记录 */
            if (ensure_data_loaded(*dataset)) {
                delete_single_record(*dataset);
            }
            break;
        case 7:
            /* 批量删除 */
            if (ensure_data_loaded(*dataset)) {
                batch_delete_records(*dataset);
            }
            break;
        case 8:
            /* 保存CSV */
            if (ensure_data_loaded(*dataset)) {
                char outfile[256];
                printf("请输入保存路径 (默认: data/processed/cleaned_data.csv): ");
                if (scanf("%255s", outfile) != 1) {
                    while (getchar() != '\n');
                    printf("输入无效。\n");
                    break;
                }
                while (getchar() != '\n');
                if (strlen(outfile) == 0) {
                    strcpy(outfile, "data/processed/cleaned_data.csv");
                }
                if (save_csv_data(outfile, *dataset)) {
                    printf("CSV数据保存成功 → %s\n", outfile);
                }
            }
            break;
        case 9:
            /* 保存二进制 */
            if (ensure_data_loaded(*dataset)) {
                char outfile[256];
                printf("请输入保存路径 (默认: data/processed/cleaned_data.bin): ");
                if (scanf("%255s", outfile) != 1) {
                    while (getchar() != '\n');
                    printf("输入无效。\n");
                    break;
                }
                while (getchar() != '\n');
                if (strlen(outfile) == 0) {
                    strcpy(outfile, "data/processed/cleaned_data.bin");
                }
                if (save_binary_data(outfile, *dataset)) {
                    printf("二进制数据保存成功 → %s\n", outfile);
                }
            }
            break;
        case 10:
            /* 存储性能对比 */
            if (ensure_data_loaded(*dataset)) {
                compare_storage_performance(*dataset);
            }
            break;
        case 11:
            /* 手动备份 */
            if (ensure_data_loaded(*dataset)) {
                backup_dataset("backup", *dataset);
            }
            break;
        case 12: {
            /* 从备份恢复 */
            char backups[64][512];
            int n = list_backup_files("backup", backups, 64);
            if (n <= 0) {
                printf("没有可用的备份文件。\n");
                break;
            }
            printf("\n可用的备份文件:\n");
            for (int i = 0; i < n; i++) {
                printf("  [%d] %s\n", i + 1, backups[i]);
            }
            printf("请选择要恢复的备份编号 (1-%d, 0取消): ", n);
            int sel;
            if (scanf("%d", &sel) != 1 || sel < 0 || sel > n) {
                while (getchar() != '\n');
                printf("选择无效。\n");
                break;
            }
            while (getchar() != '\n');
            if (sel == 0) {
                printf("已取消。\n");
                break;
            }
            if (confirm_action("恢复数据将覆盖当前内存中的数据，确认？")) {
                WaterDataset *restored = restore_dataset(backups[sel - 1]);
                if (restored) {
                    if (*dataset) free_dataset(*dataset);
                    *dataset = restored;
                }
            }
            break;
        }
        case 0:
            /* 返回主菜单 */
            break;
        default:
            printf("无效选择，请重新输入。\n");
            break;
    }
}


/* ═════════════════════════════════════════════════════════════════════
 *  模块二 子菜单
 * ═════════════════════════════════════════════════════════════════════ */

void show_preprocess_submenu(const WaterDataset *dataset) {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║      模块二：数据预处理            ║\n");
    printf("  ╠════════════════════════════════════╣\n");
    printf("  ║  [1] 检测与处理异常值 (2.1)       ║\n");
    printf("  ║  [2] 缺失值处理 (2.2)             ║\n");
    printf("  ║  [3] 移动平均滤波 (2.3)           ║\n");
    printf("  ║  [4] 保存处理后数据(CSV)          ║\n");
    printf("  ║  [5] 保存处理后数据(二进制)       ║\n");
    printf("  ║  [6] 查看预处理统计               ║\n");
    printf("  ║  [0] 返回主菜单                   ║\n");
    printf("  ╚════════════════════════════════════╝\n");
    printf("  当前数据: %s\n",
           (dataset && dataset->total_count > 0) ? "已加载" : "未加载");
    printf("  请选择操作 (0-6): ");
}

void handle_preprocess_submenu(int choice, WaterDataset **dataset) {
    switch (choice) {
        case 1:
            /* 2.1 异常值检测与处理 */
            if (!ensure_data_loaded(*dataset)) break;
            detect_and_handle_outliers(*dataset);
            (*dataset)->preprocessed = true;
            /* 处理后将统计信息写入概览文件 */
            append_preprocess_overview(*dataset);
            break;

        case 2:
            /* 2.2 缺失值处理——均值逼近法 */
            if (!ensure_data_loaded(*dataset)) break;
            fill_missing_values(*dataset);
            (*dataset)->preprocessed = true;
            /* 处理后将统计信息写入概览文件 */
            append_preprocess_overview(*dataset);
            break;

        case 3:
            /* 2.3 移动平均滤波 */
            if (!ensure_data_loaded(*dataset)) break;
            interactive_moving_average(*dataset);
            (*dataset)->preprocessed = true;
            break;

        case 4: {
            /* 保存处理后数据为CSV */
            if (!ensure_data_loaded(*dataset)) break;
            char outfile[256];
            printf("请输入保存路径 (默认: data/processed/preprocessed_data.csv): ");
            if (scanf("%255s", outfile) != 1) {
                while (getchar() != '\n');
                printf("输入无效。\n");
                break;
            }
            while (getchar() != '\n');
            if (strlen(outfile) == 0) {
                strcpy(outfile, "data/processed/preprocessed_data.csv");
            }
            if (save_csv_data(outfile, *dataset)) {
                printf("预处理数据已保存至 %s\n", outfile);
            }
            break;
        }

        case 5: {
            /* 保存处理后数据为二进制 */
            if (!ensure_data_loaded(*dataset)) break;
            char outfile[256];
            printf("请输入保存路径 (默认: data/processed/preprocessed_data.bin): ");
            if (scanf("%255s", outfile) != 1) {
                while (getchar() != '\n');
                printf("输入无效。\n");
                break;
            }
            while (getchar() != '\n');
            if (strlen(outfile) == 0) {
                strcpy(outfile, "data/processed/preprocessed_data.bin");
            }
            if (save_binary_data(outfile, *dataset)) {
                printf("预处理数据已保存至 %s\n", outfile);
            }
            break;
        }

        case 6: {
            /* 查看预处理统计信息 */
            const PreprocessStats *stats = get_preprocess_stats();
            if (!stats) {
                printf("暂无预处理统计信息。\n");
                break;
            }
            printf("\n┌──────────────────────────────────────────────────────────┐\n");
            printf("│              预处理统计信息                               │\n");
            printf("├──────────────────────────────────────────────────────────┤\n");
            printf("│  [异常值检测]                                            │\n");
            printf("│    异常数据记录数:     %8zu                          │\n",
                   stats->outlier_record_count);
            printf("│    异常参数总个数:     %8zu                          │\n",
                   stats->outlier_param_count);
            if (stats->outlier_time_start[0] != '\0') {
                printf("│    异常时间跨度:       %s ~ %s  │\n",
                       stats->outlier_time_start, stats->outlier_time_end);
            }
            printf("│    修复异常值记录数:   %8zu                          │\n",
                   stats->fixed_record_count);
            printf("│    删除异常值记录数:   %8zu                          │\n",
                   stats->deleted_record_count);
            printf("│  [缺失值处理]                                            │\n");
            printf("│    处理的缺失值个数:   %8zu                          │\n",
                   stats->missing_value_count);
            printf("└──────────────────────────────────────────────────────────┘\n");
            break;
        }

        case 0:
            /* 返回主菜单 */
            break;

        default:
            printf("无效选择，请重新输入。\n");
            break;
    }
}


/* ═════════════════════════════════════════════════════════════════════
 *  模块三 子菜单
 * ═════════════════════════════════════════════════════════════════════ */

void show_analysis_submenu(const WaterDataset *dataset) {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║      模块三：统计分析              ║\n");
    printf("  ╠════════════════════════════════════╣\n");
    printf("  ║  [1] 基本统计量 (3.1)             ║\n");
    printf("  ║  [2] 凌晨缺氧预警 (3.2)           ║\n");
    printf("  ║  [3] 盐度突变预警 (3.2)           ║\n");
    printf("  ║  [4] 相关性分析 (3.3)             ║\n");
    printf("  ║  [5] 完整分析流程                 ║\n");
    printf("  ║  [0] 返回主菜单                   ║\n");
    printf("  ╚════════════════════════════════════╝\n");
    printf("  当前数据: %s",
           (dataset && dataset->total_count > 0) ? "已加载" : "未加载");
    if (dataset && dataset->total_count > 0) {
        printf(" | 预处理: %s", dataset->preprocessed ? "✓已完成" : "✗未完成");
    }
    printf("\n");
    printf("  请选择操作 (0-5): ");
}

void handle_analysis_submenu(int choice, WaterDataset **dataset) {
    /* 分析前检查：数据是否已完成预处理 */
    if (choice >= 1 && choice <= 5 && *dataset && (*dataset)->total_count > 0
        && !(*dataset)->preprocessed) {
        printf("\n  ╔══════════════════════════════════════════════╗\n");
        printf("  ║  ⚠ 提醒：当前数据尚未进行预处理！           ║\n");
        printf("  ║  建议先执行 [模块二→数据预处理] 清洗数据，  ║\n");
        printf("  ║  否则分析结果可能包含异常值、缺失值干扰。   ║\n");
        printf("  ╠══════════════════════════════════════════════╣\n");
        printf("  ║  [Y] 继续分析（跳过预处理）                 ║\n");
        printf("  ║  [N] 返回菜单                               ║\n");
        printf("  ╚══════════════════════════════════════════════╝\n");
        printf("  请选择 (Y/N): ");
        char ans;
        scanf(" %c", &ans);
        while (getchar() != '\n');
        if (ans != 'Y' && ans != 'y') {
            printf("  已取消，请先执行数据预处理。\n");
            return;
        }
    }

    switch (choice) {
        case 1:
            /* 3.1 基本统计量 */
            if (!ensure_data_loaded(*dataset)) break;
            compute_statistics(*dataset);
            break;

        case 2:
            /* 3.2 凌晨缺氧预警 */
            if (!ensure_data_loaded(*dataset)) break;
            hypoxia_warning(*dataset);
            break;

        case 3:
            /* 3.2 盐度突变预警 */
            if (!ensure_data_loaded(*dataset)) break;
            salinity_warning(*dataset);
            break;

        case 4: {
            /* 3.3 皮尔逊相关系数矩阵 */
            if (!ensure_data_loaded(*dataset)) break;
            double matrix[6][6];
            generate_correlation_matrix(*dataset, matrix);
            break;
        }

        case 5:
            /* 完整分析流程 */
            if (!ensure_data_loaded(*dataset)) break;
            printf("\n╔══════════════════════════════════════════════════════╗\n");
            printf("║       执行完整分析流程 (3.1 → 3.2 → 3.3)             ║\n");
            printf("╚══════════════════════════════════════════════════════╝\n");
            compute_statistics(*dataset);
            hypoxia_warning(*dataset);
            salinity_warning(*dataset);
            {
                double matrix[6][6];
                generate_correlation_matrix(*dataset, matrix);
            }
            printf("\n完整分析流程已执行完毕。\n");
            printf("报告文件：\n");
            printf("  - reports/statistics_report.csv (统计量 + 相关系数矩阵)\n");
            printf("  - reports/warning_report.csv     (预警报告)\n");
            break;

        case 0:
            break;

        default:
            printf("无效选择，请重新输入。\n");
            break;
    }
}


/* ═════════════════════════════════════════════════════════════════════
 *  报告显示
 * ═════════════════════════════════════════════════════════════════════ */

void display_overview(const WaterDataset *dataset) {
    if (!dataset || dataset->total_count == 0) {
        printf("暂无数据概览。请先加载数据。\n");
        return;
    }
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║     海水养殖水质数据概览             ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║  总记录数:    %8zu             ║\n", dataset->total_count);
    printf("║  有效记录数:  %8zu             ║\n", dataset->valid_count);
    printf("║  数据有效率:  %7.2f%%            ║\n",
           dataset->total_count > 0
               ? 100.0 * dataset->valid_count / dataset->total_count
               : 0.0);
    printf("║  当前容量:    %8zu             ║\n", dataset->capacity);
    printf("╚══════════════════════════════════════╝\n");

    /* 如果有预处理统计信息，一并显示 */
    const PreprocessStats *stats = get_preprocess_stats();
    if (stats && (stats->outlier_record_count > 0 || stats->missing_value_count > 0)) {
        printf("\n┌────────── 预处理统计 ─────────────────┐\n");
        if (stats->outlier_record_count > 0) {
            printf("│ 异常数据记录数:     %8zu          │\n", stats->outlier_record_count);
            printf("│ 修复异常值记录数:   %8zu          │\n", stats->fixed_record_count);
            printf("│ 删除异常值记录数:   %8zu          │\n", stats->deleted_record_count);
        }
        if (stats->missing_value_count > 0) {
            printf("│ 处理的缺失值个数:   %8zu          │\n", stats->missing_value_count);
        }
        printf("└──────────────────────────────────────┘\n");
    }
}

void display_report_menu(void) {
    printf("报告查看功能尚未实现。\n");
}
