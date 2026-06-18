#include "auth.h"
#include <stdio.h>
#include <string.h>

bool login_user(UserRole *role) {
    const char admin_user[] = "admin";
    const char admin_pass[] = "123456";
    const char guest_user[] = "guest";
    const char guest_pass[] = "guest";

    for (int attempt = 0; attempt < 3; ++attempt) {
        char username[32];
        char password[32];
        printf("用户名: ");
        scanf("%31s", username);
        printf("密码: ");
        scanf("%31s", password);

        if (strcmp(username, admin_user) == 0 && strcmp(password, admin_pass) == 0) {
            *role = ROLE_ADMIN;
            return true;
        }
        if (strcmp(username, guest_user) == 0 && strcmp(password, guest_pass) == 0) {
            *role = ROLE_GUEST;
            return true;
        }
        printf("用户名或密码错误，请重试。\n");
    }
    return false;
}

bool has_permission(UserRole role, int feature) {
    if (role == ROLE_ADMIN) {
        return true;
    }
    /* 系统级操作（清屏、退出）对所有角色开放 */
    if (feature == 9 || feature == 0) {
        return true;
    }
    if (role == ROLE_GUEST) {
        return (feature == 5 || feature == 7);
    }
    return false;
}
