#include "draw.h"

// Functions
void start_screen();
void print_score(int score);
void print_tunnel(int *cols);
void print_tunnels(int *right_col, int *left_col);
void print_player(int x, int y);

/**
 * Print the starting message.
 */
void start_screen(void) {
    print_string(2, 5, " ____              _ _      ____            _        _                    _");
    print_string(2, 6, "|  _ \\  ___  _ __ ( ) |_   / ___| ___      / \\   ___| |__   ___  _ __ ___| |");
    print_string(2, 7, "| | | |/ _ \\| '_ \\|/| __| | |  _ / _ \\    / _ \\ / __| '_ \\ / _ \\| '__/ _ \\ |");
    print_string(2, 8, "| |_| | (_) | | | | | |_  | |_| | (_) |  / ___ \\\\__ \\ | | | (_) | | |  __/_|");
    print_string(2, 9, "|____/ \\___/|_| |_|  \\__|  \\____|\\___/  /_/   \\_\\___/_| |_|\\___/|_|  \\___(_)");

    print_string(30, 15, "Press Space to begin");
}

void draw_game() {
    gamestate state = get_state();
    switch(state) {
        case start:
            start_screen();
        case running:
            print_score(get_score());
            print_tunnels(get_leftwall(), get_rightwall());
            print_player(get_playerx(), ROWS - 2);
        case over:
            break;
    }
    print_score(get_score());
}

void print_score(int score) {
    // Print the score to the screen
    print_string(0, 0, "Score: ");
    print_string(7, 0, iota(score));
}

void print_tunnel(int *cols) {
    // Prints a column centered around the mid x coordinate
    // The offsets list deviations from the mid going top down
    // TODO fix iteration start Nick
    int i = 0;
    int color = make_color(BROWN, WHITE);
    for (; i < HEIGHT; i++) {
        print_char_c(cols[i], HEIGHT - i - 1, ' ', color);
    }
}

void print_tunnels(int *lcol, int *rcol) {
    int wall_color = make_color(BROWN, WHITE);
    int water_color = make_color(BLUE, WHITE);

    int i = 0;
    int j = 0;

    int l, r;
    int h;

    for (; i < HEIGHT; i++) {
        l = lcol[i];
        r = rcol[i];

        h = HEIGHT - i - 1;

        set_color(rcol[i], h, wall_color);
        set_color(lcol[i], h, wall_color);

        for (j = l + 1; j < r; j++) {
            set_color(j, h, water_color);
        }
    }
}

void print_player(int x, int y) {
    int raft_color = make_color(WHITE, BROWN);
    /* set_color(x, y, raft_color); */
    set_char(x, y, '^');
}
