#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "parser.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: myshell script.sh\n");
        return 1;
    }

    FILE* fp = fopen(argv[1], "r");
    if (!fp) {
        perror("open");
        return 1;
    }

    interpret(fp);
    fclose(fp);
    return 0;
}
