#include "random.h"

static unsigned long int next = 1;

int rand(int max) {
    next = next * 1103515245 + 12345;
    return (unsigned int) (next / 65536) % max;
}

void seed(unsigned int seed) {
    next = seed;
}
