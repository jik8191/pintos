#include "iota.h"

/* Converts an int to a string, max 31 chars (32 with null string)
 * We only need to support base 10 for our purposes
 */
const char *iota(int val) {
    // Converts an int to a string, max 31 chars (32 with null string)
    // We only need to support base 10 for our purposes
    int base = 10;
    static char buf[32] = {0};
    int i = 30;
    if (val == 0) {
        buf[0] = '0';
        return &buf[0];
    }
    // Go until i hits 0 or val hits 0
    // The value is updated by dividing by the base to shift it over
    for(; val && i; --i, val /= base) {
        // Getting which char it is based on its mod 10
        buf[i] = "0123456789"[val % base];
    }
    return &buf[i + 1];
}

