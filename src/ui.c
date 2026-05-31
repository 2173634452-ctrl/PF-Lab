#include "ui.h"
#include "utils.h"
#include <stdio.h>

void show_main_menu(void) {
    printf("========================================\n");
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
    (void)dataset;
    switch (choice) {
        case 1:
            printf("进入数据基础操作模块...\n");
            break;
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
            printf("显示数据概览...\n");
            break;
        case 6:
            printf("查看预警报告...\n");
            break;
        case 7:
            printf("查看分析报告...\n");
            break;
        case 8:
            printf("数据备份与恢复...\n");
            break;
        case 9:
            clear_console();
            break;
        default:
            printf("无效选择，请重新输入。\n");
            break;
    }
}

void display_overview(const WaterDataset *dataset) {
    (void)dataset;
    printf("数据概览功能尚未实现。\n");
}

void display_report_menu(void) {
    printf("报告查看功能尚未实现。\n");
}
