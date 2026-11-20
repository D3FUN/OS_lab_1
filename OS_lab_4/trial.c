#include "contracts.h"

#include <errno.h>
#include <math.h>

int prime_count(int a, int b)
{
    if (a < 1 || a > b) {
        errno = EINVAL;
        return -1;
    }
    if (b == 1) {
        return 0;
    }
    if (a == 1) {
        a = 2;
    }

    int count = 0;
    for (int n = a; ; ++n) {
        int divisor = 2;
        while (divisor < n && n % divisor != 0) {
            ++divisor;
        }
        if (divisor == n) {
            ++count;
        }
        if (n == b) {
            break;
        }
    }
    return count;
}

float e(int x)
{
    if (x <= 0) {
        errno = EINVAL;
        return NAN;
    }
    return (float)pow(1.0 + 1.0 / x, x);
}
