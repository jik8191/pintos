#include "draw.h"

// Functions
void start_screen();
void print_score(int score);
void print_tunnel(int *cols);
void print_tunnels(int *right_col, int *left_col);
void print_player(int x, int y);

void start_screen(void) {
    // The inital screen at the start of the game
    print_string(30, 7, "Don't Go Ashore!");
    print_string(28, 10, "Press Space to begin");
}

void draw_game() {
    // Draw the screen
    clear_screen();
    print_score(0);
    init_tunnel();
    int * right_wall = get_rightwall();
    int * left_wall = get_leftwall();
    print_tunnels(right_wall, left_wall);
    print_player(0, 0);
    /*print_tunnel(right_wall);*/
    /*print_tunnel(left_wall);*/
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

void print_tunnels(int *right_col, int *left_col) {
    int i = 0;
    int j = 0;
    int wall_color = make_color(BROWN, WHITE);
    int water_color = make_color(BLUE, WHITE);
    int left_bound;
    int right_bound;
    for (; i < HEIGHT; i++) {
        left_bound = left_col[i];
        right_bound = right_col[i];
        print_char_c(right_col[i], HEIGHT - i - 1, ' ', wall_color);
        print_char_c(left_col[i], HEIGHT - i - 1, ' ', wall_color);
        for (j = left_bound + 1; j < right_bound; j++) {
            print_char_c(j, HEIGHT - i - 1, ' ', water_color);
        }
    }
}

void print_player(int x, int y) {
    int water_color = make_color(BLUE, WHITE);
    print_char_c(40, 10, '@', water_color);
}
