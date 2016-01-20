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

// Functions
int make_color(int background, int foreground);
void print_char(int x, int y, char c, int color);
void print_score(int score);
void print_col(int mid, int *offsets);
const char *iota(int val);


void init_video(void) {
    /* TODO:  Do any video display initialization you might want to do, such
     *        as clearing the screen, initializing static variable state, etc.
     */
    default_background = BLUE;
    default_foreground = WHITE;
}

void clear_screen(void) {
    // Clear the screen and set it to the default background color.
    int i = 0;

    // Pointer to the video buffer
    volatile char *video = (volatile char*) VIDEO_BUFFER;

    for (i = 0; i < WIDTH * HEIGHT; i++) {
        *video++ = ' '; // Setting the char value to a space
        // Setting the color
        *video++ = make_color(default_background, default_foreground);
    }
    print_score(999);
    int offsets[25] = {0,-1,-2,-1,0,0,0,0,0,1,2,3,4,3,2,1,0,0,0,0,0,0,0,0,0};
    int offsets2[25] = {0,-1,-2,-1,0,0,0,0,0,1,2,3,4,3,2,1,0,0,0,0,0,0,0,0,0};
    print_col(25, offsets);
    print_col(35, offsets2);
}

int make_color(int background, int foreground) {
    /* Makes a color given the background and foreground */
    int color;
    color = background;
    color = color << 4; // Setting the high nibble
    color = color | foreground; // Setting the low nibble
    return color;
}

void print_char(int x, int y, char c, int color) {
    /* Print a char to a location on the screen with a given color. The screen
     * layout is such that the top left corner is (0, 0) and the bottom left
     * corner is (WIDTH - 1, HEIGHT - 1)
     */
    volatile char *video = (volatile char*) VIDEO_BUFFER;
    int index = ((y * WIDTH) + x) * 2;
    *(video + index) = c;
    *(video + index + 1) = color;
}

void print_screen(int x, int y, const char *string) {
    /* Prints a string to the screen starting from the given coordinates and
     * goes laterally.
     */
    // TODO handle when it goes beyond the y?
    while(*string != '\0') {
        print_char(x++, y, *string++,
                   make_color(default_background, default_foreground));
        if (x == WIDTH) {
            x = 0;
            y++;
        }
    }
}

void print_score(int score) {
    // Print the score to the screen
    print_screen(0, 0, "Score: ");
    print_screen(7, 0, iota(score));
}

void print_col(int mid, int *offsets) {
    // Prints a column centered around the mid x coordinate
    // The offsets list deviations from the mid going top down
    int i = 0;
    for (; i < HEIGHT; i++) {
        print_char(mid + offsets[i], i, '@', make_color(default_background, default_foreground));
    }
}

const char *iota(int val) {
    // Converts an int to a string, max 31 chars (32 with null string)
    // We only need to support base 10 for our purposes
    int base = 10;
    static char buf[32] = {0};
    int i = 30;
    // Go until i hits 0 or val hits 0
    // The value is updated by dividing by the base to shift it over
    for(; val && i; --i, val /= base) {
        // Getting which char it is based on its mod 10
        buf[i] = "0123456789"[val % base];
    }
    return &buf[i + 1];
}
