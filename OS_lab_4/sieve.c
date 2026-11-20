#include "contracts.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

int prime_count(int a, int b)
{
    if (a < 1 || a > b) {
        errno = EINVAL;
        return -1;
    }
    if (b == 1) {
        return 0;
    }
    if ((uintmax_t)b >= SIZE_MAX) {
        errno = ENOMEM;
        return -1;
    }

    size_t last = (size_t)b;
    unsigned char *composite = calloc(last + 1, sizeof *composite);
    if (composite == NULL) {
        return -1;
    }

    for (size_t p = 2; p <= last / p; ++p) {
        if (composite[p]) {
            continue;
        }
        for (size_t multiple = p * p; ; multiple += p) {
            composite[multiple] = 1;
            if (last - multiple < p) {
                break;
            }
        }
    }

    int count = 0;
    for (size_t n = a == 1 ? 2 : (size_t)a; n <= last; ++n) {
        if (!composite[n]) {
            ++count;
        }
    }
    free(composite);
    return count;
}

float e(int x)
{
    if (x <= 0) {
        errno = EINVAL;
        return NAN;
    }

    double term = 1.0;
    double sum = term;
    for (int n = 0; n < x; ++n) {
        term /= n + 1;
        double next = sum + term;
        if (next == sum) {
            break;
        }
        sum = next;
    }
    return (float)sum;
}
