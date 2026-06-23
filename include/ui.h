#ifndef UI_H
#define UI_H

#include "common.h"
#include "auth.h"

/* ── 主菜单 ── */
void show_main_menu(UserRole role);
void handle_menu_choice(int choice, WaterDataset **dataset, UserRole role);

/* ── 模块一子菜单 ── */
void show_data_submenu(const WaterDataset *dataset);
void handle_data_submenu(int choice, WaterDataset **dataset);

/* ── 模块二子菜单 ── */
void show_preprocess_submenu(const WaterDataset *dataset);
void handle_preprocess_submenu(int choice, WaterDataset **dataset);

/* ── 模块三子菜单 ── */
void show_analysis_submenu(const WaterDataset *dataset);
void handle_analysis_submenu(int choice, WaterDataset **dataset);

/* ── 模块四子菜单 ── */
void show_prediction_submenu(const WaterDataset *dataset);
void handle_prediction_submenu(int choice, WaterDataset **dataset);

/* ── 报告 ── */
void display_overview(const WaterDataset *dataset);

#endif // UI_H
