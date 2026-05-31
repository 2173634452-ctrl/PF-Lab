#ifndef UI_H
#define UI_H

#include "common.h"

void show_main_menu(void);
void handle_menu_choice(int choice, WaterDataset **dataset);
void display_overview(const WaterDataset *dataset);
void display_report_menu(void);

#endif // UI_H
