#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>

#define MAX_USERS 100

typedef enum {
    ROLE_ADMIN,
    ROLE_GUEST,
    ROLE_UNKNOWN
} UserRole;

typedef struct {
    char username[32];
    char password_hash[33];
    char salt[17];
    UserRole role;
} UserAccount;

bool login_user(UserRole *role);
bool has_permission(UserRole role, int feature);

#endif
