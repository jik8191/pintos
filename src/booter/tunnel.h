#ifndef TUNNEL_H
#define TUNNEL_H

#define COLS 80
#define ROWS 25

#define TUNNEL_WIDTH 10
#define MINWIDTH 4

#include "random.h"

void init_tunnel();
void tunnel_step();
void tunnel_shrink();

int *get_leftwall();
int *get_rightwall();

gamestate get_state();
void set_state(gamestate s);

static inline int mod(int a, int b) {
    int r = a % b;

    if (r < 0)
        r += b;

    return r;
}

typedef enum State {
    start,
    running,
    over
} gamestate;

#endif // TUNNEL_H
