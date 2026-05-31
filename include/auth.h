#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>

typedef enum {
    ROLE_ADMIN,
    ROLE_GUEST,
    ROLE_UNKNOWN
} UserRole;

bool login_user(UserRole *role);
bool has_permission(UserRole role, int feature);

#endif // AUTH_H
