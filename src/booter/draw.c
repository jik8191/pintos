#include "draw.h"

#include "video.h"
#include "state.h"
#include "iota.h"

/**
 * Print the starting message.
 */
void print_startmsg() {
    print_string(2, 5, " ____              _ _      ____            _        _                    _");
    print_string(2, 6, "|  _ \\  ___  _ __ ( ) |_   / ___| ___      / \\   ___| |__   ___  _ __ ___| |");
    print_string(2, 7, "| | | |/ _ \\| '_ \\|/| __| | |  _ / _ \\    / _ \\ / __| '_ \\ / _ \\| '__/ _ \\ |");
    print_string(2, 8, "| |_| | (_) | | | | | |_  | |_| | (_) |  / ___ \\\\__ \\ | | | (_) | | |  __/_|");
    print_string(2, 9, "|____/ \\___/|_| |_|  \\__|  \\____|\\___/  /_/   \\_\\___/_| |_|\\___/|_|  \\___(_)");

    print_string(30, 15, "Press Space to begin");
    print_string(27, 17, "Press 'A' and 'D' to move");
}

/**
 * Print the game over message.
 */
void print_gameover() {
    print_string(13, 5, "  _____                         ____                 ");
    print_string(13, 6, " / ____|                       / __ \\                ");
    print_string(13, 7, "| |  __  __ _ _ __ ___   ___  | |  | |_   _____ _ __ ");
    print_string(13, 8, "| | |_ |/ _` | '_ ` _ \\ / _ \\ | |  | \\ \\ / / _ \\ '__|");
    print_string(13, 9, "| |__| | (_| | | | | | |  __/ | |__| |\\ V /  __/ |   ");
    print_string(13, 10, " \\_____|\\__,_|_| |_| |_|\\___|  \\____/  \\_/ \\___|_|   ");

    print_string(29, 15, "Press Space to restart");
}

/**
 * Redraw the game board depending on the state.
 */
void draw_game() {
    gamestate state = get_state();

    switch(state) {
        // Start screen for the game
        case start:
            clear_chars();
            print_startmsg();
            print_tunnels(get_leftwall(), get_rightwall());
            break;

        // While the game is running
        case running:
            reset_colors();
            clear_chars();
            print_player(get_playerx(), ROWS - 2);
            print_tunnels(get_leftwall(), get_rightwall());
            break;

        // Game over screen
        case over:
            clear_chars();
            print_gameover();
            break;
    }

    print_scores();
}

/**
 * Print the scores.
 */
void print_scores() {
    print_string(0, 0, "Score: ");
    print_string(7, 0, iota(get_score()));
    print_string(0, 1, "High Score: ");
    print_string(12, 1, iota(get_highscore()));
}

/**
 * Print the tunnel walls where lcol and rcol specify the left and right walls
 * of the tunnel in arrays of column indices.
 */
void print_tunnels(int *lcol, int *rcol) {
    int wall_color = make_color(BROWN, WHITE);
    int water_color = make_color(BLUE, WHITE);

    int i = 0;
    int j = 0;

    int l, r;
    int h;

    for (; i < HEIGHT; i++) {
        l = get_wallelem(lcol, i);
        r = get_wallelem(rcol, i);

        h = HEIGHT - i - 1;

        set_color(l, h, wall_color);
        set_color(r, h, wall_color);

        // Print water between the walls.
        for (j = l + 1; j < r; j++) {
            set_color(j, h, water_color);
        }
    }
}

/**
 * Print the player character.
 */
void print_player(int x, int y) {
    int raft_color = make_color(WHITE, BROWN);
    set_char(x, y, '^');
}
