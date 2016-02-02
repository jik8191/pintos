#include "fixed_point.h"

/* Converts an integer n to a fixed point number */
FP int_to_fp(int n) {
    return n * F;
}

/* Converts a fixed point number to an integer. With the option to round
 * towards 0 or to round to the nearest. */
int fp_to_int(FP x, int to_nearest) {
    if (to_nearest) {
        if (x >= 0) {
            return (x + (F / 2)) / F;
        }
        return (x - (F / 2)) / F;
    }
    return x / F;
}
