#include "video.h"

/* This is the address of the VGA text-mode video buffer.  Note that this
 * buffer actually holds 8 pages of text, but only the first page (page 0)
 * will be displayed.
 *
 * Individual characters in text-mode VGA are represented as two adjacent
 * bytes:
 *     Byte 0 = the character value
 *     Byte 1 = the color of the character:  the high nibble is the background
 *              color, and the low nibble is the foreground color
 *
 * See http://wiki.osdev.org/Printing_to_Screen for more details.
 *
 * Also, if you decide to use a graphical video mode, the active video buffer
 * may reside at another address, and the data will definitely be in another
 * format.  It's a complicated topic.  If you are really intent on learning
 * more about this topic, go to http://wiki.osdev.org/Main_Page and look at
 * the VGA links in the "Video" section.
 */
#define VIDEO_BUFFER ((void *) 0xB8000)


/* TODO:  You can create static variables here to hold video display state,
 *        such as the current foreground and background color, a cursor
 *        position, or any other details you might want to keep track of!
 */

// Variables
static int default_background;
static int default_foreground;


void init_video(void) {
    default_background = GREEN;
    default_foreground = WHITE;
    clear_screen();
}

/**
 * Clears the screen of chars and resets to the default color.
 */
void clear_screen() {
    clear_chars();
    reset_colors();
}

/**
 * Clear all characters on the screen.
 */
void clear_chars() {
    int i = 0;

    // Pointer to the video buffer
    volatile char *video = (volatile char*) VIDEO_BUFFER;

    for (i = 0; i < WIDTH * HEIGHT; i++) {
        *video = ' ';
        video += 2;
    }
}

/**
 * Reset the screen to the default color.
 */
void reset_colors() {
    int i = 0;

    // Pointer to the video buffer
    volatile char *video = (volatile char*) VIDEO_BUFFER;

    int default_color = make_color(default_background, default_foreground);

    for (i = 0; i < WIDTH * HEIGHT; i++) {
        *(video + 1) = default_color;
        video += 2;
    }
}

/**
 * Makes a color given the background and foreground
 */
int make_color(int background, int foreground) {
    int color = background;
    color <<= 4;         // Setting the high nibble
    color |= foreground; // Setting the low nibble

    return color;
}

/**
 * Set the char at the given (x, y) coordinates.
 */
void set_char(int x, int y, char c) {
    volatile char *video = (volatile char *) VIDEO_BUFFER;

    int i = ((y * WIDTH) + x) * 2;
    *(video + i) = c;
}

/**
 * Set the color at the given (x, y) coordinates.
 */
void set_color(int x, int y, int color) {
    volatile char *video = (volatile char *) VIDEO_BUFFER;

    int i = ((y * WIDTH) + x) * 2;
    *(video + i + 1) = color;
}

/**
 * Print a char to a location on the screen with the default color.
 *
 * The screen layout is such that the top left corner is (0, 0) and the bottom
 * left corner is (WIDTH - 1, HEIGHT - 1)
 */
void print_char(int x, int y, char c) {
    int default_color = make_color(default_background, default_foreground);
    set_char(x, y, c);
    set_color(x, y, default_color);
}

/**
 * Print a char to a location on the screen with a given color.
 *
 * The screen layout is such that the top left corner is (0, 0) and the bottom
 * left corner is (WIDTH - 1, HEIGHT - 1)
 */
void print_char_c(int x, int y, char c, int color) {
    set_char(x, y, c);
    set_color(x, y, color);
}

/**
 * Prints a string to the screen starting from the given coordinates and goes
 * laterally.
 */
void print_string(int x, int y, const char *string) {
    while(*string != '\0') {
        set_char(x++, y, *string++);

        if (x == WIDTH) {
            return;
        }
    }
}

/**
 * Prints a string to the screen starting from the given coordinates and goes
 * laterally.
 */
void print_string_c(int x, int y, const char *string, int color) {
    while(*string != '\0') {
        print_char_c(x++, y, *string++, color);

        if (x == WIDTH) {
            return;
        }
    }
}
