#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

bool confirm_action(const char *prompt) {
    char choice = 'n';
    printf("%s (y/n): ", prompt);
    scanf(" %c", &choice);
    return choice == 'y' || choice == 'Y';
}

void clear_console(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}