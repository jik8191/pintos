#ifndef DRAW_H
#define DRAW_H

#include "video.h"
#include "state.h"
#include "iota.h"

void start_screen();
void draw_game();

void print_score(int score);
void print_tunnel(int *cols);
void print_tunnels(int *right_col, int *left_col);
void print_player(int x, int y);

#endif /* DRAW_H */
