#include "fixed_point.h"
#include "stdint.h"

/* Code for basic operations on fixed point numbers */

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

/* Adding to fixed point numbers */
fp fp_add(fp x, fp y) {
    fp z;
    z.int_val = x.int_val + y.int_val;
    return z;
}

/* Adding an int to a fixed point */
fp int_add(fp x, int n) {
    fp z;
    z = fp_add(x, int_to_fp(n));
    return z;
}

/* Subtract fixed point y from fixed point x */
fp fp_subtract(fp x, fp y) {
    fp z;
    z.int_val = x.int_val - y.int_val;
    return z;
}

/* Subtracting an int from a fixed point */
fp int_subtract(fp x, int n) {
    fp z;
    z = fp_subtract(x, int_to_fp(n));
    return z;
}

/* Multiply two fixed points together */
fp fp_multiply(fp x, fp y) {
    fp z;
    z.int_val = ((int64_t) x.int_val) * y.int_val / F;
    return z;
}

/* Multiply a fixed point by an int */
fp int_multiply(fp x, int n) {
    fp z;
    z.int_val = x.int_val * n;
    return z;
}

/* Divide fixed point x by fixed point y */
fp fp_divide(fp x, fp y) {
    fp z;
    z.int_val = ((int64_t) x.int_val) * F / y.int_val;
    return z;
}

/* Divided a fixed point by an integer */
fp int_divide(fp x, int n) {
    fp z;
    z.int_val = x.int_val / n;
    return z;
}
