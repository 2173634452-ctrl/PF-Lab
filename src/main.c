#include "common.h"
#include "ui.h"
#include "auth.h"
#include "data_io.h"
#include "preprocess.h"
#include "analysis.h"
#include "model.h"
#include "backup.h"
#include "utils.h"

int main(void) {
    WaterDataset *dataset = NULL;
    UserRole role = ROLE_UNKNOWN;

    if (!login_user(&role)) {
        printf("登录失败，程序退出。\n");
        return 1;
    }

    while (true) {
        show_main_menu();
        int choice = 0;
        if (scanf("%d", &choice) != 1) {
            clear_console();
            printf("输入错误，请重新输入。\n");
            while (getchar() != '\n');
            continue;
        }
        if (choice == 0) {
            printf("确认退出系统？(y/n): ");
            if (confirm_action("退出系统")) {
                break;
            }
            continue;
        }
        handle_menu_choice(choice, &dataset);
    }

    if (dataset) {
        free_dataset(dataset);
    }
    return 0;
}
