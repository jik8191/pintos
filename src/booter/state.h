#ifndef STATE_H
#define STATE_H

#define COLS 80
#define ROWS 25

// This is indexed from 0, so this is row 2.
#define PLAYER_ROW 1

#define TUNNEL_WIDTH 22
#define MINWIDTH 4

#include "random.h"
#include "draw.h"

typedef enum State {
    start,
    running,
    over
} gamestate;

void init_state();
void tunnel_step();
void tunnel_shrink();

int *get_leftwall();
int *get_rightwall();
int get_playerx();
void update_player();
int get_score();

gamestate get_state();
void set_state(gamestate s);

static inline int mod(int a, int b) {
    int r = a % b;

    if (r < 0)
        r += b;

    return r;
}

#endif // STATE_H
