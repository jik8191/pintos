#include "random.h"

// Initial seed for random number generation.
static unsigned long int next = 1;

/**
 * Generate a random number from [0, max)
 */
int rand(int max) {
    next = next * 1103515245 + 12345;
    return (unsigned int) (next / 65536) % max;
}

/**
 * Set the seed of the random number generator.
 */
void seed(unsigned int seed) {
    next = seed;
}
