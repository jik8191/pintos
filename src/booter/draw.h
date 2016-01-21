#ifndef DRAW_H
#define DRAW_H

void start_screen();
void draw_game();

void print_scores();
void print_tunnel(int *cols);
void print_tunnels(int *right_col, int *left_col);
void print_player(int x, int y);

#endif /* DRAW_H */
