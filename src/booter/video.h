#ifndef VIDEO_H
#define VIDEO_H


/* Available colors from the 16-color palette used for EGA and VGA, and
 * also for text-mode VGA output.
 */
#define BLACK          0
#define BLUE           1
#define GREEN          2
#define CYAN           3
#define RED            4
#define MAGENTA        5
#define BROWN          6
#define LIGHT_GRAY     7
#define DARK_GRAY      8
#define LIGHT_BLUE     9
#define LIGHT_GREEN   10
#define LIGHT_CYAN    11
#define LIGHT_RED     12
#define LIGHT_MAGENTA 13
#define YELLOW        14
#define WHITE         15

/* The width and height of the screen */
#define WIDTH  80
#define HEIGHT 25

#include "tunnel.h"

void init_video(void);
void clear_screen(void);
int make_color(int background, int foreground);
void print_char(int x, int y, char c);
void print_char_c(int x, int y, char c, int color);
void print_string(int x, int y, const char *string);
void print_string_c(int x, int y, const char *string, int color);


void set_char(int x, int y, char c);
void set_color(int x, int y, int color);

#endif /* VIDEO_H */
