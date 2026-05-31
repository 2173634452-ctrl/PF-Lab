#include "ui.h"
#include "utils.h"
#include "data_io.h"
#include "backup.h"
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
        case 2:
            printf("进入数据预处理模块...\n");
            break;
        case 3:
            printf("进入统计分析模块...\n");
            break;
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
    printf("  ║  [1] 加载CSV数据                  ║\n");
    printf("  ║  [2] 分页浏览数据                 ║\n");
    printf("  ║  [3] 按条件筛选                   ║\n");
    printf("  ║  [4] 按参数排序                   ║\n");
    printf("  ║  [5] 修改单条记录                 ║\n");
    printf("  ║  [6] 删除单条记录                 ║\n");
    printf("  ║  [7] 批量删除记录                 ║\n");
    printf("  ║  [8] 保存数据(CSV)                ║\n");
    printf("  ║  [9] 保存数据(二进制)             ║\n");
    printf("  ║ [10] 存储性能对比                 ║\n");
    printf("  ║ [11] 手动备份数据                 ║\n");
    printf("  ║ [12] 从备份恢复数据               ║\n");
    printf("  ║  [0] 返回主菜单                   ║\n");
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
}

void display_report_menu(void) {
    printf("报告查看功能尚未实现。\n");
}
