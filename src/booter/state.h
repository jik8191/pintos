#ifndef STATE_H
#define STATE_H

#define COLS 80
#define ROWS 25

// This is indexed from 0, so this is row 2.
#define PLAYER_ROW 1

#define TUNNEL_WIDTH 22
#define MINWIDTH 5

typedef enum State {
    start,
    running,
    over
} gamestate;

void init_state();
void tunnel_step();
void tunnel_shrink();

void lose_game();

// Accessors
int get_wallelem(int *wall, int index);
int *get_leftwall();
int *get_rightwall();
int get_playerx();
int get_score();
int get_highscore();
gamestate get_state();

// Mutators
void update_player();
void set_state(gamestate s);

/**
 * Modulo that wraps negative numbers around to the positive remainder.
 */
static inline int mod(int a, int b) {
    int r = a % b;

    if (r < 0)
        r += b;

    return r;
}

#endif // STATE_H
