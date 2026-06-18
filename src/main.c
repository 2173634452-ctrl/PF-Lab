#include "common.h"
#include "ui.h"
#include "auth.h"
#include "data_io.h"
#include "preprocess.h"
#include "analysis.h"
#include "model.h"
#include "backup.h"
#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
#ifdef _WIN32
    /* 将 Windows 控制台编码切换为 UTF-8，解决中文乱码 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    WaterDataset *dataset = NULL;
    UserRole role = ROLE_UNKNOWN;

    if (!login_user(&role)) {
        printf("登录失败，程序退出。\n");
        return 1;
    }

    while (true) {
        show_main_menu(role);
        int choice = 0;
        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请重新输入。\n");
            while (getchar() != '\n');
            clear_console();
            continue;
        }
        if (choice == 0) {
            if (confirm_action("退出系统")) {
                break;
            }
            continue;
        }
        handle_menu_choice(choice, &dataset, role);
    }

    if (dataset) {
        free_dataset(dataset);
    }
    return 0;
}
