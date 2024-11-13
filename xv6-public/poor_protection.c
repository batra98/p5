#include "types.h"
#include "user.h"

char *str = "You can't change a character!";
int main() {
    str[1] = 'O';
    printf(1,"%s\n", str);
    return 0;
}

