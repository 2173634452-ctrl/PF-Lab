#ifndef UI_H
#define UI_H

#include "common.h"

/* ── 主菜单 ── */
void show_main_menu(void);
void handle_menu_choice(int choice, WaterDataset **dataset);

/* ── 模块一子菜单 ── */
void show_data_submenu(const WaterDataset *dataset);
void handle_data_submenu(int choice, WaterDataset **dataset);

/* ── 报告 ── */
void display_overview(const WaterDataset *dataset);
void display_report_menu(void);

#endif // UI_H
