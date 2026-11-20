#include "commands.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_command(const char *line, int args[3])
{
    int count = 0;
    while (1) {
        while (isspace((unsigned char)*line)) {
            ++line;
        }
        if (*line == '\0') {
            return count;
        }
        if (count == 3) {
            return -1;
        }
        errno = 0;
        char *end;
        long value = strtol(line, &end, 10);
        if (end == line || errno == ERANGE || value < INT_MIN || value > INT_MAX ||
            (*end != '\0' && !isspace((unsigned char)*end))) {
            return -1;
        }
        args[count++] = (int)value;
        line = end;
    }
}

int run_commands(const Contracts *contracts, void (*switch_library)(void))
{
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    while ((length = getline(&line, &capacity, stdin)) >= 0) {
        int args[3];
        int count = memchr(line, '\0', (size_t)length) == NULL
                    ? parse_command(line, args) : -1;
        if (count == 0) {
            continue;
        }
        if (count == 1 && args[0] == 0 && switch_library != NULL) {
            switch_library();
        } else if (count == 3 && args[0] == 1 && args[1] >= 1 && args[1] <= args[2]) {
            int result = contracts->count(args[1], args[2]);
            if (result < 0) {
                perror("prime_count");
            } else {
                printf("%d\n", result);
            }
        } else if (count == 2 && args[0] == 2 && args[1] > 0) {
            printf("%.9g\n", contracts->approximate_e(args[1]));
        } else {
            fprintf(stderr, "Invalid command. Use 1 a b (1 <= a <= b) or 2 x (x > 0)%s.\n",
                    switch_library != NULL ? " or 0" : "");
        }
    }

    free(line);
    if (!feof(stdin) || ferror(stdin)) {
        perror("stdin");
        return EXIT_FAILURE;
    }
    if (fflush(stdout) == EOF || ferror(stdout)) {
        perror("stdout");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
