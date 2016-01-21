#include "state.h"

// The state of the game
static gamestate state = start;

static int score = 0;

// The columns of the player (always at the bottom row).
static int player;

// The index into which the next wall elements will be written, so we don't
// have to shift elements.
static int wallarr_ptr;

// Arrays representing which column the walls exist in in each row.
static int leftwall[ROWS];
static int rightwall[ROWS];

// The current width of the tunnel.
static int tunnelwidth;

/**
 * Initialize the state of the game.
 */
void init_state() {
    // Start with the same screen, but change it when the game starts.
    seed(1);

    // Player starts in the middle.
    player = COLS / 2;

    tunnelwidth = TUNNEL_WIDTH;

    // Wall is centered in the screen initially.
    leftwall[0] = (COLS - tunnelwidth - 2) / 2;
    rightwall[0] = leftwall[0] + tunnelwidth + 1;

    wallarr_ptr = 1;

    // Create the randomized walls.
    int i;
    for (i = 1; i < ROWS; i++) {
        tunnel_step();
    }

    wallarr_ptr = 0;
    score = 0;
}

/**
 * Create a new randomized row of the tunnel at the top.
 */
void tunnel_step() {
    // Check if the user will lose
    if (state == running &&
            (get_wallelem(leftwall, PLAYER_ROW+1) == player ||
             get_wallelem(rightwall, PLAYER_ROW+1) == player)) {
        state = over;
        draw_game();
        return;
    }

    // Get the position of the last row
    int last_left = get_wallelem(leftwall, -1);
    int last_right = get_wallelem(rightwall, -1);

    // Get a random number (-1, 0, 1).
    int del = rand(3) - 1;
    int left = last_left + del;
    int right = left + tunnelwidth + 1;

    // Clip for the walls
    while (left < 0) {
        left++;
        right++;
    }
    while (right >= COLS) {
        left--;
        right--;
    }

    // This is in case the tunnel shrunk and we went left, this would leave an
    // open gap in the wall.
    while (right < last_right - 1) {
        left++;
        right++;
    }

    leftwall[wallarr_ptr] = left;
    rightwall[wallarr_ptr] = right;

    wallarr_ptr = mod(wallarr_ptr + 1, ROWS);

    score++;

    draw_game();
}

/**
 * Move a player in a given direction if possible.
 *
 * This might result in the player losing. Pass in a 1 for a move to the right
 * and a -1 for a move to the left. All other inputs will be ignored. The game
 * must also be currently running for this to work.
 */
void update_player(int direction) {
    // Don't do anything unless we are running.
    if (state != running) {
        return;
    }

    // Don't do anything for invalid moves.
    if (direction != 1 && direction != -1) {
        return;
    }


    int lft = get_wallelem(leftwall, PLAYER_ROW);
    int rht = get_wallelem(rightwall, PLAYER_ROW);

    int newx = player + direction;

    if (newx >= rht || newx <= lft) {
        state = over;
    } else {
        player = newx;
    }

    draw_game();
}

/**
 * Shrink the size of the tunnel, down to a minimum.
 */
void tunnel_shrink() {
    tunnelwidth -= 1;

    while (tunnelwidth < MINWIDTH) {
        tunnelwidth++;
    }
}

/**
 * Returns the element corresponding to the index, indexed from 0 starting from
 * the bottom row of the screen.
 */
int get_wallelem(int *wall, int index) {
    int i = mod(wallarr_ptr + index, ROWS);
    return wall[i];
}

/**
 * Accessor for the left wall of the tunnel.
 */
int *get_leftwall() {
    return leftwall;
}

/**
 * Accessor for the right wall of the tunnel.
 */
int *get_rightwall() {
    return rightwall;
}

/**
 * Return the x position of the player.
 */
int get_playerx() {
    return player;
}

gamestate get_state() {
    return state;
}

void set_state(gamestate s) {
    state = s;
}

int get_score() {
    return score;
}
