#ifndef LAB4_COMMANDS_H
#define LAB4_COMMANDS_H

typedef struct {
    int (*count)(int, int);
    float (*approximate_e)(int);
} Contracts;

int run_commands(const Contracts *contracts, void (*switch_library)(void));

#endif
