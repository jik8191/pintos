#include "tunnel.h"

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
void init_tunnel() {
    // Player starts in the middle.
    player = COLS / 2;

    //tunnelwidth = TUNNEL_WIDTH;
    tunnelwidth = 10;

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
}

/**
 * Create a new randomized row of the tunnel at the top.
 */
void tunnel_step() {
    // Get the position of the last row
    int last_idx = mod(wallarr_ptr - 1, ROWS);
    int last_left = leftwall[last_idx];

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

    leftwall[wallarr_ptr] = left;
    rightwall[wallarr_ptr] = right;

    wallarr_ptr = mod(wallarr_ptr + 1, ROWS);
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
