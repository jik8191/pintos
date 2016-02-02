#ifndef __LIB_KERNEL_FIXED_POINT_H
#define __LIB_KERNEL_FIXED_POINT_H

#define F 16384 // 2**14

typedef struct {
    int int_val;
} fp;

/* Conversions */
fp int_to_fp(int n);
int fp_to_int(fp x, int to_nearest);

/* Adding and subtracting */
fp fp_add(fp x, fp y);
fp int_add(fp x, int n);
fp fp_subtract(fp x, fp y);
fp int_subtract(fp x, int n);

/* Multiplying and dividing */
fp fp_multiply(fp x, fp y);
fp int_multiply(fp x, int n);
fp fp_divide(fp x, fp y);
fp int_divide(fp x, int n);

#endif /* lib/kernel/fixed_point.h */
