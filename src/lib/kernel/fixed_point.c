#include "fixed_point.h"

/* Converts an integer n to a fixed point number */
fp int_to_fp(int n) {
    fp x;
    x.int_val = n * F;
    return x;
}

/* Converts a fixed point number to an integer. With the option to round
 * towards 0 or to round to the nearest. */
int fp_to_int(fp x, int to_nearest) {
    if (to_nearest) {
        if (x.int_val >= 0) {
            return (x.int_val + (F / 2)) / F;
        }
        return (x.int_val - (F / 2)) / F;
    }
    return x.int_val / F;
}
