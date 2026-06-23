#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define USERS_FILE "data/users.dat"

static UserAccount users[MAX_USERS];
static int user_count = 0;
static int rand_seeded = 0;

/* MD5 implementation follows RFC 1321 */

typedef struct {
    unsigned int state[4];
    unsigned int count[2];
    unsigned char buffer[64];
} MD5Context;

static unsigned int MD5_F(unsigned int x, unsigned int y, unsigned int z) {
    return (x & y) | (~x & z);
}

static unsigned int MD5_G(unsigned int x, unsigned int y, unsigned int z) {
    return (x & z) | (y & ~z);
}

static unsigned int MD5_H(unsigned int x, unsigned int y, unsigned int z) {
    return x ^ y ^ z;
}

static unsigned int MD5_I(unsigned int x, unsigned int y, unsigned int z) {
    return y ^ (x | ~z);
}

static unsigned int left_rotate(unsigned int x, unsigned int n) {
    return (x << n) | (x >> (32 - n));
}

static const unsigned int md5_table[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const unsigned int md5_shift[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

static void md5_transform(unsigned int state[4], const unsigned char block[64]) {
    unsigned int a = state[0], b = state[1], c = state[2], d = state[3];
    unsigned int x[16];

    for (int i = 0, j = 0; i < 16; i++, j += 4) {
        x[i] = (unsigned int)block[j]
             | ((unsigned int)block[j + 1] << 8)
             | ((unsigned int)block[j + 2] << 16)
             | ((unsigned int)block[j + 3] << 24);
    }

    for (int i = 0; i < 64; i++) {
        unsigned int f, k;
        if (i < 16) {
            f = MD5_F(b, c, d);
            k = i;
        } else if (i < 32) {
            f = MD5_G(b, c, d);
            k = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = MD5_H(b, c, d);
            k = (3 * i + 5) % 16;
        } else {
            f = MD5_I(b, c, d);
            k = (7 * i) % 16;
        }
        unsigned int temp = d;
        d = c;
        c = b;
        b = b + left_rotate(a + f + x[k] + md5_table[i], md5_shift[i]);
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void md5_init(MD5Context *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

static void md5_update(MD5Context *ctx, const unsigned char *input, unsigned int input_len) {
    unsigned int index = (ctx->count[0] >> 3) & 0x3F;
    ctx->count[0] += input_len << 3;
    if (ctx->count[0] < (input_len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += input_len >> 29;

    unsigned int part_len = 64 - index;
    unsigned int i = 0;

    if (input_len >= part_len) {
        memcpy(ctx->buffer + index, input, part_len);
        md5_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63 < input_len; i += 64) {
            md5_transform(ctx->state, input + i);
        }
        index = 0;
    }

    memcpy(ctx->buffer + index, input + i, input_len - i);
}

static void md5_final(unsigned char digest[16], MD5Context *ctx) {
    unsigned char padding[64] = { 0x80 };
    unsigned char bits[8];

    for (int i = 0; i < 8; i++) {
        bits[i] = (unsigned char)(ctx->count[i > 3 ? 1 : 0] >> ((i % 4) * 8));
    }

    unsigned int index = (ctx->count[0] >> 3) & 0x3F;
    unsigned int pad_len = (index < 56) ? (56 - index) : (120 - index);
    md5_update(ctx, padding, pad_len);
    md5_update(ctx, bits, 8);

    for (int i = 0; i < 16; i++) {
        digest[i] = (unsigned char)(ctx->state[i >> 2] >> ((i % 4) * 8));
    }
}

static void md5_string(const char *input, char hex_output[33]) {
    MD5Context ctx;
    unsigned char digest[16];
    md5_init(&ctx);
    md5_update(&ctx, (const unsigned char *)input, (unsigned int)strlen(input));
    md5_final(digest, &ctx);
    for (int i = 0; i < 16; i++) {
        sprintf(hex_output + i * 2, "%02x", digest[i]);
    }
    hex_output[32] = '\0';
}

static void ensure_rand_seeded(void) {
    if (!rand_seeded) {
        srand((unsigned int)time(NULL));
        rand_seeded = 1;
    }
}

static void generate_salt(char salt[17]) {
    ensure_rand_seeded();
    for (int i = 0; i < 16; i++) {
        salt[i] = "0123456789abcdef"[rand() % 16];
    }
    salt[16] = '\0';
}

static void hash_password(const char *password, const char *salt, char hash_hex[33]) {
    char combined[128];
    snprintf(combined, sizeof(combined), "%s%s", password, salt);
    md5_string(combined, hash_hex);
}

static int load_users(void) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) {
        /* First run: create the file with default accounts */
        fp = fopen(USERS_FILE, "w");
        if (!fp) return 0;

        char salt[17];
        char hash[33];

        generate_salt(salt);
        hash_password("123456", salt, hash);
        fprintf(fp, "admin|%s|%s|0\n", hash, salt);

        generate_salt(salt);
        hash_password("guest", salt, hash);
        fprintf(fp, "guest|%s|%s|1\n", hash, salt);

        fclose(fp);

        fp = fopen(USERS_FILE, "r");
        if (!fp) return 0;
    }

    user_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp) && user_count < MAX_USERS) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        UserAccount u;
        int role_int;
        if (sscanf(line, "%31[^|]|%32[^|]|%16[^|]|%d",
                   u.username, u.password_hash, u.salt, &role_int) == 4) {
            u.role = (UserRole)role_int;
            users[user_count++] = u;
        }
    }
    fclose(fp);
    return user_count;
}

static int save_user_to_file(const UserAccount *u) {
    FILE *fp = fopen(USERS_FILE, "a");
    if (!fp) return 0;
    fprintf(fp, "%s|%s|%s|%d\n", u->username, u->password_hash, u->salt, (int)u->role);
    fclose(fp);
    return 1;
}

static int username_exists(const char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) return 1;
    }
    return 0;
}

static void register_user(void) {
    if (user_count >= MAX_USERS) {
        printf("用户数量已达上限，无法注册新用户。\n");
        return;
    }

    UserAccount new_user;
    char confirm_password[32];

    printf("\n--- 用户注册 ---\n");

    while (1) {
        printf("请输入新用户名: ");
        if (scanf("%31s", new_user.username) != 1) {
            while (getchar() != '\n');
            printf("输入无效。\n");
            continue;
        }
        if (strlen(new_user.username) == 0) {
            printf("用户名不能为空。\n");
            continue;
        }
        if (username_exists(new_user.username)) {
            printf("用户名已存在，请换一个。\n");
            continue;
        }
        break;
    }

    while (1) {
        printf("请输入密码: ");
        if (scanf("%31s", confirm_password) != 1) {
            while (getchar() != '\n');
            printf("输入无效。\n");
            continue;
        }
        if (strlen(confirm_password) < 1) {
            printf("密码不能为空。\n");
            continue;
        }
        printf("请再次输入密码: ");
        char password2[32];
        if (scanf("%31s", password2) != 1) {
            while (getchar() != '\n');
            printf("输入无效。\n");
            continue;
        }
        if (strcmp(confirm_password, password2) != 0) {
            printf("两次密码不一致，请重试。\n");
            continue;
        }
        break;
    }

    new_user.role = ROLE_GUEST;
    generate_salt(new_user.salt);
    hash_password(confirm_password, new_user.salt, new_user.password_hash);

    if (!save_user_to_file(&new_user)) {
        printf("注册失败：无法写入用户文件。\n");
        return;
    }

    users[user_count++] = new_user;
    printf("注册成功！用户名: %s，角色: 访客\n", new_user.username);
}

bool login_user(UserRole *role) {
    if (load_users() == 0) {
        printf("错误：无法加载用户数据。\n");
        return false;
    }

    int attempts = 0;
    while (attempts < 3) {
        char username[32];
        printf("\n用户名 (输入 register 注册新账户): ");
        if (scanf("%31s", username) != 1) {
            while (getchar() != '\n');
            printf("输入无效。\n");
            continue;
        }

        if (strcmp(username, "register") == 0) {
            register_user();
            continue;
        }

        char password[32];
        printf("密码: ");
        if (scanf("%31s", password) != 1) {
            while (getchar() != '\n');
            printf("输入无效。\n");
            continue;
        }

        for (int i = 0; i < user_count; i++) {
            if (strcmp(username, users[i].username) == 0) {
                char hash[33];
                hash_password(password, users[i].salt, hash);
                if (strcmp(hash, users[i].password_hash) == 0) {
                    *role = users[i].role;
                    printf("登录成功！当前角色: %s\n",
                           (*role == ROLE_ADMIN) ? "管理员" : "访客");
                    return true;
                }
                break;
            }
        }

        attempts++;
        if (attempts < 3) {
            printf("用户名或密码错误，请重试（剩余 %d 次机会）。\n", 3 - attempts);
        }
    }

    printf("登录失败次数过多，程序退出。\n");
    return false;
}

bool has_permission(UserRole role, int feature) {
    if (role == ROLE_ADMIN) {
        return true;
    }
    if (feature == 9 || feature == 0) {
        return true;
    }
    if (role == ROLE_GUEST) {
        return (feature == 5 || feature == 7);
    }
    return false;
}
